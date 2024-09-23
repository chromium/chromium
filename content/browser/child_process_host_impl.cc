// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/child_process_host_impl.h"

#include <limits>
#include <tuple>

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
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/common/content_constants_internal.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/browser/child_process_host_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/message_filter.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/constants.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "content/browser/mac_helpers.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {

// Global atomic to generate child process unique IDs.
base::AtomicSequenceNumber g_unique_id;

}  // namespace

namespace content {

ChildProcessHost::~ChildProcessHost() = default;

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  if (child_path.empty() && flags & CHILD_ALLOW_SELF) {
    child_path = base::FilePath(base::kProcSelfExe);
  }
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  if (child_path.empty()) {
    base::PathService::Get(CHILD_PROCESS_EXE, &child_path);
  }

#if BUILDFLAG(IS_MAC)
  std::string child_base_name = child_path.BaseName().value();

  if (flags != CHILD_NORMAL && base::apple::AmIBundled()) {
    // This is a specialized helper, with the |child_path| at
    // ../Framework.framework/Versions/X/Helpers/Chromium Helper.app/Contents/
    // MacOS/Chromium Helper. Go back up to the "Helpers" directory to select
    // a different variant.
    child_path = child_path.DirName().DirName().DirName().DirName();

    if (flags == CHILD_RENDERER) {
      child_base_name += kMacHelperSuffix_renderer;
    } else if (flags == CHILD_GPU) {
      child_base_name += kMacHelperSuffix_gpu;
    } else if (flags == CHILD_PLUGIN) {
      child_base_name += kMacHelperSuffix_plugin;
    } else if (flags > CHILD_EMBEDDER_FIRST) {
      child_base_name +=
          GetContentClient()->browser()->GetChildProcessSuffix(flags);
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    child_path = child_path.Append(child_base_name + ".app")
                     .Append("Contents")
                     .Append("MacOS")
                     .Append(child_base_name);
  }
#endif  // BUILDFLAG(IS_MAC)

  return child_path;
}

ChildProcessHostImpl::ChildProcessHostImpl(ChildProcessHostDelegate* delegate,
                                           IpcMode ipc_mode)
    : ipc_mode_(ipc_mode), delegate_(delegate), opening_channel_(false) {
  if (ipc_mode_ == IpcMode::kLegacy) {
    // In legacy mode, we only have an IPC Channel. Bind ChildProcess to a
    // disconnected pipe so it quietly discards messages.
    std::ignore = child_process_.BindNewPipeAndPassReceiver();
    channel_ = IPC::ChannelMojo::Create(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessReceiverAttachmentName),
        IPC::Channel::MODE_SERVER, this,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  } else if (ipc_mode_ == IpcMode::kNormal) {
    child_process_.Bind(mojo::PendingRemote<mojom::ChildProcess>(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessReceiverAttachmentName),
        /*version=*/0));
    receiver_.Bind(mojo::PendingReceiver<mojom::ChildProcessHost>(
        mojo_invitation_->AttachMessagePipe(
            kChildProcessHostRemoteAttachmentName)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&ChildProcessHostImpl::OnDisconnectedFromChildProcess,
                       base::Unretained(this)));
  }
}

ChildProcessHostImpl::~ChildProcessHostImpl() {
  // If a channel was never created than it wasn't registered and the filters
  // weren't notified. For the sake of symmetry don't call the matching teardown
  // functions. This is analogous to how RenderProcessHostImpl handles things.
  if (!channel_) {
    return;
  }

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  for (auto& filter : filters_) {
    filter->OnChannelClosing();
    filter->OnFilterRemoved();
  }
#endif
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
void ChildProcessHostImpl::AddFilter(IPC::MessageFilter* filter) {
  filters_.push_back(filter);

  if (channel_) {
    filter->OnFilterAdded(channel_.get());
  }
}
#endif

void ChildProcessHostImpl::BindReceiver(mojo::GenericPendingReceiver receiver) {
  child_process_->BindReceiver(std::move(receiver));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChildProcessHostImpl::ReinitializeLogging(
    uint32_t logging_dest,
    base::ScopedFD log_file_descriptor) {
  auto logging_settings = mojom::LoggingSettings::New();
  logging_settings->logging_dest = logging_dest;
  logging_settings->log_file_descriptor =
      mojo::PlatformHandle(std::move(log_file_descriptor));
  child_process()->ReinitializeLogging(std::move(logging_settings));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::Process& ChildProcessHostImpl::GetPeerProcess() {
  if (!peer_process_.IsValid()) {
    const base::Process& process = delegate_->GetProcess();
    if (process.IsValid()) {
      peer_process_ = base::Process::OpenWithExtraPrivileges(process.Pid());
      if (!peer_process_.IsValid()) {
        peer_process_ = process.Duplicate();
      }
      DCHECK(peer_process_.IsValid());
    }
  }

  return peer_process_;
}

void ChildProcessHostImpl::ForceShutdown() {
  child_process_->ProcessShutdown();
}

std::optional<mojo::OutgoingInvitation>&
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

    mojo::ScopedMessagePipeHandle bootstrap =
        mojo_invitation_->AttachMessagePipe(kLegacyIpcBootstrapAttachmentName);
    channel_ = IPC::ChannelMojo::Create(
        std::move(bootstrap), IPC::Channel::MODE_SERVER, this,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
  DCHECK(channel_);

  // Since we're initializing a legacy IPC Channel, we will use its connection
  // status to monitor child process lifetime instead of using the status of the
  // `receiver_` endpoint.
  if (receiver_.is_bound()) {
    receiver_.set_disconnect_handler(base::NullCallback());
  }

  bool initialized = InitChannel();
  DCHECK(initialized);
}

bool ChildProcessHostImpl::InitChannel() {
  if (!channel_->Connect()) {
    return false;
  }

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  for (auto& filter : filters_) {
    filter->OnFilterAdded(channel_.get());
  }
#endif

  delegate_->OnChannelInitialized(channel_.get());

  // Make sure these messages get sent first.
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::GetInstance()->Enabled();
  child_process_->SetIPCLoggingEnabled(enabled);
#endif

  opening_channel_ = true;

  return true;
}

void ChildProcessHostImpl::OnDisconnectedFromChildProcess() {
  if (channel_) {
    opening_channel_ = false;
    delegate_->OnChannelError();
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    for (auto& filter : filters_) {
      filter->OnChannelError();
    }
#endif
  }

  // This will delete host_, which will also destroy this!
  delegate_->OnChildDisconnected();
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

// static
int ChildProcessHost::GenerateChildProcessUniqueId() {
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
          switches::kSingleProcess)) {
    return memory_instrumentation::mojom::kServiceTracingProcessId;
  }

  // The hash value is incremented so that the tracing id is never equal to
  // MemoryDumpManager::kInvalidTracingProcessId.
  return static_cast<uint64_t>(
             base::PersistentHash(base::byte_span_from_ref(child_process_id))) +
         1;
}

void ChildProcessHostImpl::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void ChildProcessHostImpl::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  delegate_->BindHostReceiver(std::move(receiver));
}

bool ChildProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging* logger = IPC::Logging::GetInstance();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return true;
  }

  if (logger->Enabled()) {
    logger->OnPreDispatchMessage(msg);
  }
#endif  // IPC_MESSAGE_LOG_ENABLED

  bool handled = false;
  for (auto& filter : filters_) {
    if (filter->OnMessageReceived(msg)) {
      handled = true;
      break;
    }
  }

  if (!handled) {
    handled = delegate_->OnMessageReceived(msg);
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (logger->Enabled()) {
    logger->OnPostDispatchMessage(msg);
  }
#endif  // IPC_MESSAGE_LOG_ENABLED
  return handled;
#else
  return false;
#endif  // CONTENT_ENABLE_LEGACY_IPC
}

void ChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  // Propagate the pseudonymization salt to all the child processes.
  //
  // Doing this as the first step in this method helps to minimize scenarios
  // where child process runs code that depends on the pseudonymization salt
  // before it has been set.  See also https://crbug.com/1479308#c5
  //
  // TODO(dullweber, lukasza): Figure out if it is possible to reset the salt
  // at a regular interval (on the order of hours?).  The browser would need
  // to be responsible for 1) deciding when the refresh happens and 2) pushing
  // the updated salt to all the child processes.
  child_process_->SetPseudonymizationSalt(GetPseudonymizationSalt());

  // We ignore the `peer_pid` argument, which ultimately comes over IPC from the
  // remote process, in favor of the PID already known by the browser after
  // launching the process. This is partly because IPC Channel is being phased
  // out and some process types no longer use it, but also because there's
  // really no need to get this information from the child process when we
  // already have it.
  //
  // TODO(crbug.com/41256971): Remove the peer_pid argument altogether from
  // IPC::Listener::OnChannelConnected.
  const base::Process& peer_process = GetPeerProcess();
  base::ProcessId pid =
      peer_process.IsValid() ? peer_process.Pid() : base::GetCurrentProcId();
  opening_channel_ = false;
  delegate_->OnChannelConnected(pid);
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  for (auto& filter : filters_) {
    filter->OnChannelConnected(pid);
  }
#endif
}

void ChildProcessHostImpl::OnChannelError() {
  OnDisconnectedFromChildProcess();
}

void ChildProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  delegate_->OnBadMessageReceived(message);
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void ChildProcessHostImpl::DumpProfilingData(base::OnceClosure callback) {
  child_process_->WriteClangProfilingProfile(std::move(callback));
}

void ChildProcessHostImpl::SetProfilingFile(base::File file) {
  child_process_->SetProfilingFile(std::move(file));
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Notifies the child process of memory pressure level.
void ChildProcessHostImpl::NotifyMemoryPressureToChildProcess(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  child_process()->OnMemoryPressure(level);
}
#endif

void ChildProcessHostImpl::SetBatterySaverMode(
    bool battery_saver_mode_enabled) {
  child_process()->SetBatterySaverMode(battery_saver_mode_enabled);
}

}  // namespace content
