// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_TOOL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_TOOL_H_

#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMWindow;
class ScriptObject;
class ScriptState;
class ScriptValue;

// The web-exposed representation of a tool registered via WebMCP, returned by
// `ModelContext.getTools()`. See `tool.idl`.
//
// A `Tool` is an immutable snapshot: it holds a copy of the
// `mojom::blink::ScriptTool` as it existed when `getTools()` resolved, and
// nothing mutates it afterwards.
//
// Critically, a `Tool` carries the non-web-exposed `tool_id` of the tool it
// describes. `ModelContext.executeTool()` routes on that identifier rather
// than on the tool's name, so a caller holding a `Tool` for a tool that has
// since been unregistered will fail to execute, instead of silently executing
// a *different* tool that happens to have since been registered under the same
// name.
class CORE_EXPORT Tool final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // `window` is the (non-null) window of the document that registered the
  // tool; `script_tool` is a snapshot of the tool's state. `input_schema` is
  // `script_tool->input_schema` already parsed into a script value, and is
  // empty iff `script_tool->input_schema` is null.
  Tool(DOMWindow* window,
       mojom::blink::ScriptToolPtr script_tool,
       const ScriptValue& input_schema);
  ~Tool() override;

  // Not web-exposed. The stable identity of the underlying registered tool.
  const base::UnguessableToken& ToolId() const {
    return script_tool_->tool_id.value();
  }

  // Web-exposed attributes. See `tool.idl`.
  const String& name() const;
  const String& description() const;
  // Returns a null `String` if the tool was registered without a title, which
  // the bindings expose to script as the empty string.
  const String& title() const;
  // Null once the registering window has been garbage collected. See
  // `window_`.
  DOMWindow* window() const;
  String origin() const;
  // Both of these return null if the tool was registered without the
  // corresponding member.
  ScriptObject inputSchema(ScriptState* script_state) const;
  ScriptObject annotations(ScriptState* script_state) const;

  // The `[CachedAttribute]` invalidation predicate for `inputSchema` and
  // `annotations`. `this` is an immutable snapshot, so the cached V8 values are
  // never stale.
  bool IsSnapshotDirty() const { return false; }

  void Trace(Visitor* visitor) const override;

 private:
  // The window of the document that registered this tool.
  //
  // This is weak so that a retained `Tool` doesn't keep that window (and, for a
  // same-process window, its `Document`) alive after the registering frame
  // navigates away or is removed.
  //
  // This doesn't make GC timing observable to script. While the window is its
  // frame's current window, the frame keeps it alive. So it can only be
  // collected after it has been disconnected from its frame, which already
  // makes it appear to script as `null` (see `DOMWindow::Wrap()`), and makes
  // `ModelContext::executeTool()` reject. Collection changes nothing further
  // that script can see.
  WeakMember<DOMWindow> window_;
  mojom::blink::ScriptToolPtr script_tool_;
  TraceWrapperV8Reference<v8::Value> input_schema_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_TOOL_H_
