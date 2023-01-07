// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_PROFILING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_PROFILING_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/native_profiling.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
namespace protocol {

// Handle the DevTool native profiling commands. This is currently used only
// for the PGO instrumented build.
class NativeProfilingHandler final : public DevToolsDomainHandler,
                                     public NativeProfiling::Backend {
 public:
  NativeProfilingHandler();
  NativeProfilingHandler(const NativeProfilingHandler& other) = delete;
  NativeProfilingHandler& operator=(const NativeProfilingHandler& other) =
      delete;
  ~NativeProfilingHandler() override;

  // DevToolsDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // NativeProfiling::Backend.
  void DumpProfilingDataOfAllProcesses(
      std::unique_ptr<DumpProfilingDataOfAllProcessesCallback> callback)
      override;

 private:
  std::unique_ptr<NativeProfiling::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_PROFILING_HANDLER_H_
