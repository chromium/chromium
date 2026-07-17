// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEBUGGER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEBUGGER_HANDLER_H_

#include "content/browser/devtools/protocol/debugger.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {
namespace protocol {

class DebuggerHandler : public DevToolsDomainHandler, public Debugger::Backend {
 public:
  DebuggerHandler();

  DebuggerHandler(const DebuggerHandler&) = delete;
  DebuggerHandler& operator=(const DebuggerHandler&) = delete;

  ~DebuggerHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  Response SetBreakpointByUrl(
      int in_lineNumber,
      std::optional<String> in_url,
      std::optional<String> in_urlRegex,
      std::optional<String> in_scriptHash,
      std::optional<int> in_columnNumber,
      std::optional<String> in_condition,
      String* out_breakpointId,
      std::unique_ptr<protocol::Array<protocol::Debugger::Location>>*
          out_locations) override;
  Response RemoveBreakpoint(const String& in_breakpointId) override;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEBUGGER_HANDLER_H_
