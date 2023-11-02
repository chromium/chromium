// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/browser.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/visual_debugger.h"

namespace content {
namespace protocol {

class VisualDebuggerHandler : public DevToolsDomainHandler,
                              public VisualDebugger::Backend {
 public:
  VisualDebuggerHandler();
  ~VisualDebuggerHandler() override;

 private:
  // DevToolsDomainHandler:
  void Wire(UberDispatcher* dispatcher) override;

  DispatchResponse FilterStream(
      std::unique_ptr<base::Value::Dict> in_filter) override;

  DispatchResponse StartStream() override;
  DispatchResponse StopStream() override;

  void OnFrameResponse(base::Value json);

  bool enabled_ = false;
  std::unique_ptr<VisualDebugger::Frontend> frontend_;
  base::WeakPtrFactory<VisualDebuggerHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_VISUAL_DEBUGGER_HANDLER_H_
