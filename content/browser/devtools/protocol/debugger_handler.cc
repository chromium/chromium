// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/debugger_handler.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/devtools_session.h"

namespace content {
namespace protocol {

DebuggerHandler::DebuggerHandler()
    : DevToolsDomainHandler(Debugger::Metainfo::domainName) {}

DebuggerHandler::~DebuggerHandler() = default;

void DebuggerHandler::Wire(UberDispatcher* dispatcher) {
  Debugger::Dispatcher::wire(dispatcher, this);
}

Response DebuggerHandler::SetBreakpointByUrl(
    int in_lineNumber,
    std::optional<String> in_url,
    std::optional<String> in_urlRegex,
    std::optional<String> in_scriptHash,
    std::optional<int> in_columnNumber,
    std::optional<String> in_condition,
    String* out_breakpointId,
    std::unique_ptr<protocol::Array<protocol::Debugger::Location>>*
        out_locations) {
  int type = 0;
  std::string selector;
  blink::mojom::URLBreakpointLocatorPtr locator;
  int selector_count = (in_urlRegex.has_value() ? 1 : 0) +
                       (in_url.has_value() ? 1 : 0) +
                       (in_scriptHash.has_value() ? 1 : 0);
  if (selector_count != 1) {
    // This should have been `InvalidParams`, but the tests expect `ServerError`
    // at the moment. Ditto for imperfect wording.
    return Response::ServerError(
        "Either url or urlRegex or scriptHash must be specified.");
  }
  if (in_urlRegex.has_value()) {
    type = 2;
    selector = in_urlRegex.value();
    locator =
        blink::mojom::URLBreakpointLocator::NewUrlRegex(in_urlRegex.value());
  } else if (in_url.has_value()) {
    type = 1;
    selector = in_url.value();
    locator = blink::mojom::URLBreakpointLocator::NewUrl(in_url.value());
  } else if (in_scriptHash.has_value()) {
    type = 3;
    selector = in_scriptHash.value();
    locator = blink::mojom::URLBreakpointLocator::NewScriptHash(
        in_scriptHash.value());
  } else {
    NOTREACHED();  // Per conditional bail out above.
  }

  std::string breakpoint_id =
      "br:" + base::NumberToString(type) + ":" +
      base::NumberToString(in_lineNumber) + ":" +
      base::NumberToString(in_columnNumber.value_or(0)) + ":" + selector;

  blink::mojom::BrowserOriginatingSessionState* state =
      session()->browser_originating_session_state();
  auto bp = blink::mojom::URLBreakpoint::New();
  bp->line_number = in_lineNumber;
  bp->locator = std::move(locator);
  bp->column_number = in_columnNumber;
  bp->condition = in_condition;
  state->url_breakpoints[breakpoint_id] = std::move(bp);

  return Response::FallThrough(std::move(breakpoint_id));
}

Response DebuggerHandler::RemoveBreakpoint(const String& in_breakpointId) {
  blink::mojom::BrowserOriginatingSessionState* state =
      session()->browser_originating_session_state();
  auto it = state->url_breakpoints.find(in_breakpointId);
  if (it != state->url_breakpoints.end()) {
    state->url_breakpoints.erase(it);
  }
  return Response::FallThrough();
}

}  // namespace protocol
}  // namespace content
