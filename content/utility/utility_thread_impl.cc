// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/debug/crash_logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/browser_exposed_utility_interfaces.h"
#include "content/utility/services.h"
#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {

namespace {

class ServiceBinderImpl {
 public:
  explicit ServiceBinderImpl(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(std::move(main_thread_task_runner)) {}
  ~ServiceBinderImpl() = default;

  void BindServiceInterface(mojo::GenericPendingReceiver* receiver) {
    // Set a crash key so utility process crash reports indicate which service
    // was running in the process.
    static auto* service_name_crash_key = base::debug::AllocateCrashKeyString(
        "service-name", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(service_name_crash_key,
                                   receiver->interface_name().value());

    // We watch for and terminate on PEER_CLOSED, but we also terminate if the
    // watcher is cancelled (meaning the local endpoint was closed rather than
    // the peer). Hence any breakage of the service pipe leads to termination.
    auto watcher = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    watcher->Watch(receiver->pipe(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                   MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                   base::BindRepeating(&ServiceBinderImpl::OnServicePipeClosed,
                                       base::Unretained(this), watcher.get()));
    service_pipe_watchers_.insert(std::move(watcher));
    HandleServiceRequestOnIOThread(std::move(*receiver),
                                   main_thread_task_runner_.get());
  }

  static base::Optional<ServiceBinderImpl>& GetInstanceStorage() {
    static base::NoDestructor<base::Optional<ServiceBinderImpl>> storage;
    return *storage;
  }

 private:
  void OnServicePipeClosed(mojo::SimpleWatcher* which,
                           MojoResult result,
                           const mojo::HandleSignalsState& state) {
    // NOTE: It doesn't matter whether this was peer closure or local closure,
    // and those are the only two ways this method can be invoked.

    auto it = service_pipe_watchers_.find(which);
    DCHECK(it != service_pipe_watchers_.end());
    service_pipe_watchers_.erase(it);

    // No more services running in this process.
    if (service_pipe_watchers_.empty()) {
      main_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ServiceBinderImpl::ShutDownProcess));
    }
  }

  static void ShutDownProcess() {
    // Ensure that shutdown also tears down |this|. This is necessary to support
    // multiple tests in the same test suite using out-of-process services via
    // the InProcessUtilityThreadHelper.
    GetInstanceStorage().reset();
    UtilityThread::Get()->ReleaseProcess();
  }

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // These trap signals on any (unowned) primordial service pipes. We don't
  // actually care about the signals so these never get armed. We only watch for
  // cancellation, because that means the service's primordial pipe handle was
  // closed locally and we treat that as the service calling it quits.
  std::set<std::unique_ptr<mojo::SimpleWatcher>, base::UniquePtrComparator>
      service_pipe_watchers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceBinderImpl);
};

ChildThreadImpl::Options::ServiceBinder GetServiceBinder() {
  auto& storage = ServiceBinderImpl::GetInstanceStorage();
  // NOTE: This may already be initialized from a previous call if we're in
  // single-process mode.
  if (!storage)
    storage.emplace(base::ThreadTaskRunnerHandle::Get());
  return base::BindRepeating(&ServiceBinderImpl::BindServiceInterface,
                             base::Unretained(&storage.value()));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure),
                      ChildThreadImpl::Options::Builder()
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(base::DoNothing(),
                      ChildThreadImpl::Options::Builder()
                          .InBrowserProcess(params)
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Close the channel to cause the UtilityProcessHost to be deleted. We need to
  // take a different code path than the multi-process case because that case
  // depends on the child process going away to close the channel, but that
  // can't happen when we're in single process mode.
  channel()->Close();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/false);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
void UtilityThreadImpl::EnsureBlinkInitializedWithSandboxSupport() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/true);
}
#endif

void UtilityThreadImpl::EnsureBlinkInitializedInternal(bool sandbox_support) {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_ =
      sandbox_support
          ? std::make_unique<UtilityBlinkPlatformWithSandboxSupportImpl>()
          : std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  GetContentClient()->utility()->UtilityThreadStarted();

  // NOTE: Do not add new interfaces directly within this method. Instead,
  // modify the definition of |ExposeUtilityInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  content::ExposeUtilityInterfacesToBrowser(&binders);
  ExposeInterfacesToBrowser(std::move(binders));

  service_factory_.reset(new UtilityServiceFactory);
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return GetContentClient()->utility()->OnMessageReceived(msg);
}

void UtilityThreadImpl::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  DCHECK(service_factory_);
  service_factory_->RunService(service_name, std::move(receiver));
}

}  // namespace content
