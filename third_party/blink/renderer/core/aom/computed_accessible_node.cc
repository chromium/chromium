// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

class ComputedAccessibleNodePromiseResolver::RequestAnimationFrameCallback final
    : public FrameCallback {
 public:
  explicit RequestAnimationFrameCallback(
      ComputedAccessibleNodePromiseResolver* resolver)
      : resolver_(resolver) {}

  RequestAnimationFrameCallback(const RequestAnimationFrameCallback&) = delete;
  RequestAnimationFrameCallback& operator=(
      const RequestAnimationFrameCallback&) = delete;

  void Invoke(double) override {
    resolver_->continue_callback_request_id_ = 0;
    resolver_->UpdateTreeAndResolve();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    FrameCallback::Trace(visitor);
  }

 private:
  Member<ComputedAccessibleNodePromiseResolver> resolver_;
};

ComputedAccessibleNodePromiseResolver::ComputedAccessibleNodePromiseResolver(
    ScriptState* script_state,
    Document& document,
    AXID ax_id)
    : ax_id_(ax_id),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      ax_context_(std::make_unique<AXContext>(document, ui::kAXModeComplete)) {
  DCHECK(ax_id);
}

ComputedAccessibleNodePromiseResolver::ComputedAccessibleNodePromiseResolver(
    ScriptState* script_state,
    Element& element)
    : element_(element),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      ax_context_(std::make_unique<AXContext>(element.GetDocument(),
                                              ui::kAXModeComplete)) {}

ScriptPromise ComputedAccessibleNodePromiseResolver::Promise() {
  return resolver_->Promise();
}

void ComputedAccessibleNodePromiseResolver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(resolver_);
}

void ComputedAccessibleNodePromiseResolver::ComputeAccessibleNode() {
  resolve_with_node_ = true;
  EnsureUpToDate();
}

void ComputedAccessibleNodePromiseResolver::EnsureUpToDate() {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
  if (continue_callback_request_id_ || !ax_context_->GetDocument())
    return;
  // TODO(aboxhall): Trigger a call when lifecycle is next at kPrePaintClean.
  RequestAnimationFrameCallback* callback =
      MakeGarbageCollected<RequestAnimationFrameCallback>(this);
  continue_callback_request_id_ =
      ax_context_->GetDocument()->RequestAnimationFrame(callback);
}

void ComputedAccessibleNodePromiseResolver::UpdateTreeAndResolve() {
  if (!ax_context_->GetDocument()) {
    resolver_->Resolve();
    return;
  }
  LocalFrame* local_frame = ax_context_->GetDocument()->GetFrame();
  if (!local_frame) {
    resolver_->Resolve();
    return;
  }
  WebLocalFrameClient* client =
      WebLocalFrameImpl::FromFrame(local_frame)->Client();
  WebComputedAXTree* tree = client->GetOrCreateWebComputedAXTree();
  tree->ComputeAccessibilityTree();

  if (!resolve_with_node_) {
    resolver_->Resolve();
    return;
  }

  ax_context_->GetDocument()->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);
  AXObjectCache& cache = ax_context_->GetAXObjectCache();
  AXID ax_id = ax_id_ ? ax_id_ : cache.GetAXID(element_);
  if (!ax_id || !cache.ObjectFromAXID(ax_id)) {
    resolver_->Resolve();  // No AXObject exists for this element.
    return;
  }

  ComputedAccessibleNode* accessible_node =
      ax_context_->GetDocument()->GetOrCreateComputedAccessibleNode(ax_id);
  DCHECK(accessible_node);
  resolver_->Resolve(accessible_node);
}

// ComputedAccessibleNode ------------------------------------------------------

ComputedAccessibleNode::ComputedAccessibleNode(AXID ax_id, Document* document)
    : ax_id_(ax_id),
      ax_context_(std::make_unique<AXContext>(*document, ui::kAXModeComplete)) {
}

ComputedAccessibleNode::~ComputedAccessibleNode() = default;

absl::optional<bool> ComputedAccessibleNode::atomic() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_ATOMIC);
}

absl::optional<bool> ComputedAccessibleNode::busy() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_BUSY);
}

absl::optional<bool> ComputedAccessibleNode::disabled() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_DISABLED);
}

absl::optional<bool> ComputedAccessibleNode::readOnly() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_READONLY);
}

absl::optional<bool> ComputedAccessibleNode::expanded() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_EXPANDED);
}

absl::optional<bool> ComputedAccessibleNode::modal() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MODAL);
}

absl::optional<bool> ComputedAccessibleNode::multiline() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MULTILINE);
}

absl::optional<bool> ComputedAccessibleNode::multiselectable() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MULTISELECTABLE);
}

absl::optional<bool> ComputedAccessibleNode::required() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_REQUIRED);
}

absl::optional<bool> ComputedAccessibleNode::selected() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_SELECTED);
}

absl::optional<int32_t> ComputedAccessibleNode::colCount() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_COUNT);
}

absl::optional<int32_t> ComputedAccessibleNode::colIndex() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_INDEX);
}

absl::optional<int32_t> ComputedAccessibleNode::colSpan() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_SPAN);
}

absl::optional<int32_t> ComputedAccessibleNode::level() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_HIERARCHICAL_LEVEL);
}

absl::optional<int32_t> ComputedAccessibleNode::posInSet() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_POS_IN_SET);
}

absl::optional<int32_t> ComputedAccessibleNode::rowCount() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_COUNT);
}

absl::optional<int32_t> ComputedAccessibleNode::rowIndex() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_INDEX);
}

absl::optional<int32_t> ComputedAccessibleNode::rowSpan() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_SPAN);
}

absl::optional<int32_t> ComputedAccessibleNode::setSize() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_SET_SIZE);
}

absl::optional<float> ComputedAccessibleNode::valueMax() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_MAX);
}

absl::optional<float> ComputedAccessibleNode::valueMin() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_MIN);
}

absl::optional<float> ComputedAccessibleNode::valueNow() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_NOW);
}

ScriptPromise ComputedAccessibleNode::ensureUpToDate(
    ScriptState* script_state) {
  if (!GetDocument())
    return ScriptPromise();  // Empty promise.
  auto* resolver = MakeGarbageCollected<ComputedAccessibleNodePromiseResolver>(
      script_state, *GetDocument(), ax_id_);
  ScriptPromise promise = resolver->Promise();
  resolver->EnsureUpToDate();
  return promise;
}

const String ComputedAccessibleNode::autocomplete() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_AUTOCOMPLETE);
}

const String ComputedAccessibleNode::checked() const {
  WebString out;

  if (GetTree() && GetTree()->GetCheckedStateForAXNode(ax_id_, &out)) {
    return out;
  }
  return String();
}

const String ComputedAccessibleNode::keyShortcuts() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_KEY_SHORTCUTS);
}
const String ComputedAccessibleNode::name() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_NAME);
}
const String ComputedAccessibleNode::placeholder() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_PLACEHOLDER);
}

const String ComputedAccessibleNode::role() const {
  WebString out;
  if (GetTree() && GetTree()->GetRoleForAXNode(ax_id_, &out)) {
    return out;
  }
  return String();
}

const String ComputedAccessibleNode::roleDescription() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_ROLE_DESCRIPTION);
}

const String ComputedAccessibleNode::valueText() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_VALUE_TEXT);
}

ComputedAccessibleNode* ComputedAccessibleNode::parent() const {
  WebComputedAXTree* tree = GetTree();
  if (!tree) {
    return nullptr;
  }
  int32_t parent_ax_id;
  if (!tree->GetParentIdForAXNode(ax_id_, &parent_ax_id)) {
    return nullptr;
  }
  return GetDocument()->GetOrCreateComputedAccessibleNode(parent_ax_id);
}

ComputedAccessibleNode* ComputedAccessibleNode::firstChild() const {
  WebComputedAXTree* tree = GetTree();
  if (!tree) {
    return nullptr;
  }
  int32_t child_ax_id;
  if (!tree->GetFirstChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return GetDocument()->GetOrCreateComputedAccessibleNode(child_ax_id);
}

ComputedAccessibleNode* ComputedAccessibleNode::lastChild() const {
  WebComputedAXTree* tree = GetTree();
  if (!tree) {
    return nullptr;
  }
  int32_t child_ax_id;
  if (!tree->GetLastChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return GetDocument()->GetOrCreateComputedAccessibleNode(child_ax_id);
}

ComputedAccessibleNode* ComputedAccessibleNode::previousSibling() const {
  WebComputedAXTree* tree = GetTree();
  if (!tree) {
    return nullptr;
  }
  int32_t sibling_ax_id;
  if (!tree->GetPreviousSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return GetDocument()->GetOrCreateComputedAccessibleNode(sibling_ax_id);
}

ComputedAccessibleNode* ComputedAccessibleNode::nextSibling() const {
  WebComputedAXTree* tree = GetTree();
  if (!tree) {
    return nullptr;
  }
  int32_t sibling_ax_id;
  if (!tree->GetNextSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return GetDocument()->GetOrCreateComputedAccessibleNode(sibling_ax_id);
}

Document* ComputedAccessibleNode::GetDocument() const {
  return ax_context_->GetDocument();
}

WebComputedAXTree* ComputedAccessibleNode::GetTree() const {
  if (!GetDocument())
    return nullptr;
  LocalFrame* local_frame = GetDocument()->GetFrame();
  if (!local_frame)
    return nullptr;

  WebLocalFrameClient* client =
      WebLocalFrameImpl::FromFrame(local_frame)->Client();
  return client->GetOrCreateWebComputedAXTree();
}

absl::optional<bool> ComputedAccessibleNode::GetBoolAttribute(
    WebAOMBoolAttribute attr) const {
  bool value;
  if (GetTree() && GetTree()->GetBoolAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return absl::nullopt;
}

absl::optional<int32_t> ComputedAccessibleNode::GetIntAttribute(
    WebAOMIntAttribute attr) const {
  int32_t value;
  if (GetTree() && GetTree()->GetIntAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return absl::nullopt;
}

absl::optional<float> ComputedAccessibleNode::GetFloatAttribute(
    WebAOMFloatAttribute attr) const {
  float value;
  if (GetTree() && GetTree()->GetFloatAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return absl::nullopt;
}

const String ComputedAccessibleNode::GetStringAttribute(
    WebAOMStringAttribute attr) const {
  WebString out;
  if (GetTree() && GetTree()->GetStringAttributeForAXNode(ax_id_, attr, &out)) {
    return out;
  }
  return String();
}

void ComputedAccessibleNode::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
