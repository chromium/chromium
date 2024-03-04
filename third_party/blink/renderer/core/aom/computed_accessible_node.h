// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_

#include "third_party/blink/public/platform/web_computed_ax_tree.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class ScriptState;

class ComputedAccessibleNodePromiseResolver final
    : public GarbageCollected<ComputedAccessibleNodePromiseResolver> {
 public:
  ComputedAccessibleNodePromiseResolver(ScriptState*, Document&, AXID);
  ComputedAccessibleNodePromiseResolver(ScriptState*, Element&);
  ~ComputedAccessibleNodePromiseResolver() {}

  ScriptPromiseTyped<ComputedAccessibleNode> Promise();
  void ComputeAccessibleNode();
  void EnsureUpToDate();
  void Trace(Visitor*) const;

 private:
  void UpdateTreeAndResolve();
  class RequestAnimationFrameCallback;

  int continue_callback_request_id_ = 0;

  // Backed by either element_ or ax_id_.
  Member<Element> element_;
  AXID ax_id_;

  Member<ScriptPromiseResolverTyped<ComputedAccessibleNode>> resolver_;
  bool resolve_with_node_ = false;
  std::unique_ptr<AXContext> ax_context_;
};

class ComputedAccessibleNode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CORE_EXPORT ComputedAccessibleNode(AXID, Document*);
  ~ComputedAccessibleNode() override;

  void Trace(Visitor*) const override;

  // TODO(meredithl): add accessors for state properties.
  std::optional<bool> atomic() const;
  std::optional<bool> busy() const;
  std::optional<bool> disabled() const;
  std::optional<bool> readOnly() const;
  std::optional<bool> expanded() const;
  std::optional<bool> modal() const;
  std::optional<bool> multiline() const;
  std::optional<bool> multiselectable() const;
  std::optional<bool> required() const;
  std::optional<bool> selected() const;

  std::optional<int32_t> colCount() const;
  std::optional<int32_t> colIndex() const;
  std::optional<int32_t> colSpan() const;
  std::optional<int32_t> level() const;
  std::optional<int32_t> posInSet() const;
  std::optional<int32_t> rowCount() const;
  std::optional<int32_t> rowIndex() const;
  std::optional<int32_t> rowSpan() const;
  std::optional<int32_t> setSize() const;

  std::optional<float> valueMax() const;
  std::optional<float> valueMin() const;
  std::optional<float> valueNow() const;

  const String autocomplete() const;
  const String checked() const;
  const String keyShortcuts() const;
  const String name() const;
  const String placeholder() const;
  const String role() const;
  const String roleDescription() const;
  const String valueText() const;

  ComputedAccessibleNode* parent() const;
  ComputedAccessibleNode* firstChild() const;
  ComputedAccessibleNode* lastChild() const;
  ComputedAccessibleNode* previousSibling() const;
  ComputedAccessibleNode* nextSibling() const;

  ScriptPromiseTyped<ComputedAccessibleNode> ensureUpToDate(ScriptState*);

 private:
  Document* GetDocument() const;
  WebComputedAXTree* GetTree() const;
  std::optional<bool> GetBoolAttribute(WebAOMBoolAttribute) const;
  std::optional<int32_t> GetIntAttribute(WebAOMIntAttribute) const;
  std::optional<float> GetFloatAttribute(WebAOMFloatAttribute) const;
  const String GetStringAttribute(WebAOMStringAttribute) const;

  AXID ax_id_;

  // This tree is owned by the RenderFrame.
  std::unique_ptr<AXContext> ax_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_
