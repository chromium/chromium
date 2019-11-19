// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/public/utility/utility_thread.h"
#include "third_party/blink/public/platform/platform.h"

namespace content {
class UtilityServiceFactory;

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

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

  // Helper to handle incoming RunService calls.
  std::unique_ptr<UtilityServiceFactory> service_factory_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThreadImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
