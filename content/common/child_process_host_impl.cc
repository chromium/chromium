// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_host_impl.h"

#include <limits>

#include "base/atomic_sequence_num.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/child_process_host_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/constants.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/linux_util.h"
#elif defined(OS_MAC)
#include "base/mac/foundation_util.h"
#include "content/common/mac_helpers.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "content/public/common/profiling_utils.h"
#endif

namespace {

// Global atomic to generate child process unique IDs.
base::AtomicSequenceNumber g_unique_id;

}  // namespace

namespace content {

// static
std::unique_ptr<ChildProcessHost> ChildProcessHost::Create(
    ChildProcessHostDelegate* delegate,
    IpcMode ipc_mode) {
  return base::WrapUnique(new ChildProcessHostImpl(delegate, ipc_mode));
}

// static
base::FilePath ChildProcessHost::GetChildPath(int flags) {
  base::FilePath child_path;

  child_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kBrowserSubprocessPath);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  if (child_path.empty() && flags & CHILD_ALLOW_SELF)
    child_path = base::FilePath(base::kProcSelfExe);
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  if (child_path.empty())
    base::PathService::Get(CHILD_PROCESS_EXE, &child_path);

#if defined(OS_MAC)
  std::string child_base_name = child_path.BaseName().value();

  if (flags != CHILD_NORMAL && base::mac::AmIBundled()
#if defined(ARCH_CPU_ARM64)
      && flags != CHILD_LAUNCH_X86_64
#endif  // ARCH_CPU_ARM64
  ) {
    // This is a specialized helper, with the |child_path| at
    // ../Framework.framework/Versions/X/Helpers/Chromium Helper.app/Contents/
    // MacOS/Chromium Helper. Go back up to the "Helpers" directory to select
    // a different variant.
    child_path = child_path.DirName().DirName().DirName().DirName();

    if (flags == CHILD_RENDERER) {
      child_base_name += kMacHelperSuffix_renderer;
    } else if (flags == CHILD_GPU) {
      child_base_name += kMacHelperSuffix_gpu;
#if BUILDFLAG(ENABLE_PLUGINS)
    } else if (flags == CHILD_PLUGIN) {
      child_base_name += kMacHelperSuffix_plugin;
#endif
    } else {
      NOTREACHED();
    }

    child_path = child_path.Append(child_base_name + ".app")
                     .Append("Contents")
                     .Append("MacOS")
                     .Append(child_base_name);
  }
#endif

  return child_path;
}

ChildProcessHostImpl::ChildProcessHostImpl(ChildProcessHostDelegate* delegate,
                                           IpcMode ipc_mode)
    : ipc_mode_(ipc_mode), delegate_(delegate), opening_channel_(false) {
  if (ipc_mode_ == IpcMode::kLegacy) {
    // In legacy mode, we only have an IPC Channel. Bind ChildProcess to a
    // disconnected pipe so it quietly discards messages.
    ignore_result(child_process_.BindNewPipeAndPassReceiver());
    channel_ = IPC::ChannelMojo::Create(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessReceiverAttachmentName),
        IPC::Channel::MODE_SERVER, this, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        mojo::internal::MessageQuotaChecker::MaybeCreate());
  } else if (ipc_mode_ == IpcMode::kNormal) {
    child_process_.Bind(mojo::PendingRemote<mojom::ChildProcess>(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessReceiverAttachmentName),
        /*version=*/0));
    receiver_.Bind(mojo::PendingReceiver<mojom::ChildProcessHost>(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessHostRemoteAttachmentName)));
  }
}

ChildProcessHostImpl::~ChildProcessHostImpl() {
  // If a channel was never created than it wasn't registered and the filters
  // weren't notified. For the sake of symmetry don't call the matching teardown
  // functions. This is analogous to how RenderProcessHostImpl handles things.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }
}

void ChildProcessHostImpl::AddFilter(IPC::MessageFilter* filter) {
  filters_.push_back(filter);

  if (channel_)
    filter->OnFilterAdded(channel_.get());
}

void ChildProcessHostImpl::BindReceiver(mojo::GenericPendingReceiver receiver) {
  child_process_->BindReceiver(std::move(receiver));
}

void ChildProcessHostImpl::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  child_process_->RunService(service_name, std::move(receiver));
}

void ChildProcessHostImpl::ForceShutdown() {
  child_process_->ProcessShutdown();
}

base::Optional<mojo::OutgoingInvitation>&
ChildProcessHostImpl::GetMojoInvitation() {
  return mojo_invitation_;
}

void ChildProcessHostImpl::CreateChannelMojo() {
  // If in legacy mode, |channel_| is already initialized by the constructor
  // not bound through the ChildProcess API.
  if (ipc_mode_ != IpcMode::kLegacy) {
    DCHECK(!channel_);
    DCHECK_EQ(ipc_mode_, IpcMode::kNormal);
    DCHECK(child_process_);

    mojo::PendingRemote<IPC::mojom::ChannelBootstrap> bootstrap;
    auto bootstrap_receiver = bootstrap.InitWithNewPipeAndPassReceiver();
    child_process_->BootstrapLegacyIpc(std::move(bootstrap_receiver));
    channel_ = IPC::ChannelMojo::Create(
        bootstrap.PassPipe(), IPC::Channel::MODE_SERVER, this,
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        mojo::internal::MessageQuotaChecker::MaybeCreate());
  }
  DCHECK(channel_);

  bool initialized = InitChannel();
  DCHECK(initialized);
}

bool ChildProcessHostImpl::InitChannel() {
  if (!channel_->Connect())
    return false;

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());

  delegate_->OnChannelInitialized(channel_.get());

  // Make sure these messages get sent first.
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::GetInstance()->Enabled();
  child_process_->SetIPCLoggingEnabled(enabled);
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  child_process_->SetProfilingFile(OpenProfilingFile());
#endif

  opening_channel_ = true;

  return true;
}

bool ChildProcessHostImpl::IsChannelOpening() {
  return opening_channel_;
}

bool ChildProcessHostImpl::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }
  return channel_->Send(message);
}

int ChildProcessHostImpl::GenerateChildProcessUniqueId() {
  // This function must be threadsafe.
  //
  // Historically, this function returned ids started with 1, so in several
  // places in the code a value of 0 (rather than kInvalidUniqueID) was used as
  // an invalid value. So we retain those semantics.
  int id = g_unique_id.GetNext() + 1;

  CHECK_NE(0, id);
  CHECK_NE(kInvalidUniqueID, id);

  return id;
}

uint64_t ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
    int child_process_id) {
  // In single process mode, all the children are hosted in the same process,
  // therefore the generated memory dump guids should not be conditioned by the
  // child process id. The clients need not be aware of SPM and the conversion
  // takes care of the SPM special case while translating child process ids to
  // tracing process ids.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return memory_instrumentation::mojom::kServiceTracingProcessId;

  // The hash value is incremented so that the tracing id is never equal to
  // MemoryDumpManager::kInvalidTracingProcessId.
  return static_cast<uint64_t>(base::PersistentHash(
             base::as_bytes(base::make_span(&child_process_id, 1)))) +
         1;
}

void ChildProcessHostImpl::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  delegate_->BindHostReceiver(std::move(receiver));
}

bool ChildProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging* logger = IPC::Logging::GetInstance();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return true;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool handled = false;
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(msg)) {
      handled = true;
      break;
    }
  }

  if (!handled) {
      handled = delegate_->OnMessageReceived(msg);
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg);
#endif
  return handled;
}

void ChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  if (!peer_process_.IsValid()) {
    peer_process_ = base::Process::OpenWithExtraPrivileges(peer_pid);
    if (!peer_process_.IsValid())
       peer_process_ = delegate_->GetProcess().Duplicate();
    DCHECK(peer_process_.IsValid());
  }
  opening_channel_ = false;
  delegate_->OnChannelConnected(peer_pid);
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);
}

void ChildProcessHostImpl::OnChannelError() {
  opening_channel_ = false;
  delegate_->OnChannelError();

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // This will delete host_, which will also destroy this!
  delegate_->OnChildDisconnected();
}

void ChildProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  delegate_->OnBadMessageReceived(message);
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void ChildProcessHostImpl::DumpProfilingData(base::OnceClosure callback) {
  child_process_->WriteClangProfilingProfile(std::move(callback));
}
#endif

}  // namespace content
