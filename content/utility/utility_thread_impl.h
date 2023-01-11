// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/process/current_process.h"
#include "base/strings/pattern.h"
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

  UtilityThreadImpl(const UtilityThreadImpl&) = delete;
  UtilityThreadImpl& operator=(const UtilityThreadImpl&) = delete;

  ~UtilityThreadImpl() override;
  void Shutdown() override;

  // UtilityThread:
  void ReleaseProcess() override;
  void EnsureBlinkInitialized() override;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
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
  void RunServiceDeprecated(
      const std::string& service_name,
      mojo::ScopedMessagePipeHandle service_pipe) override;

  // blink::Platform implementation if needed.
  std::unique_ptr<blink::Platform> blink_platform_impl_;

  // Helper to handle incoming RunServiceDeprecated calls. Note that this is
  // deprecated and only remains in support of some embedders which haven't
  // migrated away from Service Manager-based services yet.
  std::unique_ptr<UtilityServiceFactory> service_factory_;

  // The ServiceFactory used to handle incoming service requests from a
  // browser-side ServiceProcessHost. Any service registered here will run on
  // the main thread of its service process.
  std::unique_ptr<mojo::ServiceFactory> main_thread_services_;
};

using CurrentProcessType = base::CurrentProcessType;

struct ServiceCurrentProcessType {
  const char* name;
  CurrentProcessType type;
};

CurrentProcessType GetCurrentProcessType(const std::string& name);

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
