// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class ScriptState;

class ComputedAccessibleNodePromiseResolver final
    : public GarbageCollected<ComputedAccessibleNodePromiseResolver> {
 public:
  ComputedAccessibleNodePromiseResolver(ScriptState*, Element&);
  ~ComputedAccessibleNodePromiseResolver() {}

  ScriptPromise Promise();
  void ComputeAccessibleNode();
  void EnsureUpToDate();
  void Trace(blink::Visitor*);

 private:
  void UpdateTreeAndResolve();
  class RequestAnimationFrameCallback;

  int continue_callback_request_id_ = 0;
  Member<Element> element_;
  Member<ScriptPromiseResolver> resolver_;
  bool resolve_with_node_;
  std::unique_ptr<AXContext> ax_context_;
};

class ComputedAccessibleNode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ComputedAccessibleNode(AXID, WebComputedAXTree*, Document*);
  ~ComputedAccessibleNode() override;

  void Trace(Visitor*) override;

  // TODO(meredithl): add accessors for state properties.
  bool atomic(bool& is_null) const;
  bool busy(bool& is_null) const;
  bool disabled(bool& is_null) const;
  bool readOnly(bool& is_null) const;
  bool expanded(bool& is_null) const;
  bool modal(bool& is_null) const;
  bool multiline(bool& is_null) const;
  bool multiselectable(bool& is_null) const;
  bool required(bool& is_null) const;
  bool selected(bool& is_null) const;

  int32_t colCount(bool& is_null) const;
  int32_t colIndex(bool& is_null) const;
  int32_t colSpan(bool& is_null) const;
  int32_t level(bool& is_null) const;
  int32_t posInSet(bool& is_null) const;
  int32_t rowCount(bool& is_null) const;
  int32_t rowIndex(bool& is_null) const;
  int32_t rowSpan(bool& is_null) const;
  int32_t setSize(bool& is_null) const;

  float valueMax(bool& is_null) const;
  float valueMin(bool& is_null) const;
  float valueNow(bool& is_null) const;

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

  ScriptPromise ensureUpToDate(ScriptState*);

 private:
  bool GetBoolAttribute(WebAOMBoolAttribute, bool& is_null) const;
  int32_t GetIntAttribute(WebAOMIntAttribute, bool& is_null) const;
  float GetFloatAttribute(WebAOMFloatAttribute, bool& is_null) const;
  const String GetStringAttribute(WebAOMStringAttribute) const;

  AXID ax_id_;

  // This tree is owned by the RenderFrame.
  blink::WebComputedAXTree* tree_;
  Member<Document> document_;
  std::unique_ptr<AXContext> ax_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_COMPUTED_ACCESSIBLE_NODE_H_
