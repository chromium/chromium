// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/browser.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/visual_debugger.h"

namespace content {

class GpuProcessHost;

namespace protocol {

class VisualDebuggerHandler : public DevToolsDomainHandler,
                              public VisualDebugger::Backend {
 public:
  VisualDebuggerHandler();
  ~VisualDebuggerHandler() override;

 private:
  friend class VisualDebuggerHandlerTest;

  // DevToolsDomainHandler:
  void Wire(UberDispatcher* dispatcher) override;

  DispatchResponse FilterStream(
      std::unique_ptr<base::DictValue> in_filter) override;

  DispatchResponse StartStream() override;
  DispatchResponse StopStream() override;

  void OnFrameResponse(base::Value json);

  GpuProcessHost* GetGpuProcessHost(bool force_create);

  // Optional override used by tests to simulate an unavailable GPU process.
  base::RepeatingCallback<GpuProcessHost*(bool)>
      gpu_process_host_getter_for_testing_;
  bool enabled_ = false;
  std::unique_ptr<VisualDebugger::Frontend> frontend_;
  base::WeakPtrFactory<VisualDebuggerHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_
