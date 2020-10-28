// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "third_party/blink/public/platform/platform.h"

namespace mojo {
class ServiceFactory;
}

namespace content {
class UtilityServiceFactory;

// This class represents the background thread where the utility task runs.
class UtilityThreadImpl : public UtilityThread,
                          public ChildThreadImpl {
 public:
  explicit UtilityThreadImpl(base::RepeatingClosure quit_closure);
  // Constructor used when running in single process mode.
  explicit UtilityThreadImpl(const InProcessChildThreadParams& params);
  ~UtilityThreadImpl() override;
  void Shutdown() override;

  // UtilityThread:
  void ReleaseProcess() override;
  void EnsureBlinkInitialized() override;
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  void EnsureBlinkInitializedWithSandboxSupport() override;
#endif

  // Handles an incoming service interface receiver from a browser-side
  // ServiceProcessHost. This is called only if `receiver` didn't first match
  // any registered IO-thread service handlers in this process. If successful,
  // `termination_callback` will eventually be invoked when the new service
  // instance terminates.
  //
  // If there is no matching service, `receiver` is discarded and
  // `termination_callback` is invoked immediately.
  void HandleServiceRequest(mojo::GenericPendingReceiver receiver,
                            base::OnceClosure termination_callback);

 private:
  void EnsureBlinkInitializedInternal(bool sandbox_support);
  void Init();

  // ChildThreadImpl:
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver) override;

  // blink::Platform implementation if needed.
  std::unique_ptr<blink::Platform> blink_platform_impl_;

  // Helper to handle incoming RunService calls. Note that this is deprecated
  // and only remains in support of some embedders which haven't migrated away
  // from Service Manager-based services yet.
  std::unique_ptr<UtilityServiceFactory> service_factory_;

  // The ServiceFactory used to handle incoming service requests from a
  // browser-side ServiceProcessHost. Any service registered here will run on
  // the main thread of its service process.
  std::unique_ptr<mojo::ServiceFactory> main_thread_services_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThreadImpl);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
