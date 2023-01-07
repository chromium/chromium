// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class DeferredShapingDisallowScope;
class DeferredShapingMinimumTopScope;
class DeferredShapingViewportScope;
class Document;
class Element;
class LayoutObject;
class LocalFrameView;
class NGLayoutInputNode;
class Node;

enum class ReshapeReason {
  kComputedStyle,
  kDomContentLoaded,  // DOMCntentLoaded after FCP
  kFcp,               // FCP after DOMContentLoaded
  kFragmentAnchor,
  kFocus,
  kGeometryApi,
  kInspector,
  kLastResort,
  kPrinting,
  kScrollingApi,
  kTesting,
};

// DeferredShapingController class manages states of the Deferred Shaping
// feature.
//
// A LayoutView owns a DeferredShapingController instance. A LayoutView and
// its DeferredShapingController are created and destroyed together.
class CORE_EXPORT DeferredShapingController
    : public GarbageCollected<DeferredShapingController> {
 public:
  // This returns nullptr if the |document| is not active.
  static DeferredShapingController* From(const Document& document);
  static DeferredShapingController& From(const NGLayoutInputNode input_node);
  explicit DeferredShapingController(Document& document);
  void Trace(Visitor* visitor) const;

  // Disable deferred shaping on the frame persistently.
  // This function should not be called during laying out.
  void DisallowDeferredShaping();
  bool DefaultAllowDeferredShaping() const {
    return default_allow_deferred_shaping_;
  }

  // Manage states during layout

  // The bottom position of the nearest scrollable ancestor.
  // This returns kIndefiniteSize if the viewport bottom is not registered.
  LayoutUnit CurrentViewportBottom() const { return current_viewport_bottom_; }
  void SetCurrentViewportBottom(base::PassKey<DeferredShapingViewportScope>,
                                LayoutUnit value) {
    current_viewport_bottom_ = value;
  }
  // The "minimum top" position of the box which is being laid out.
  LayoutUnit CurrentMinimumTop() const { return current_minimum_top_; }
  void SetCurrentMinimumTop(base::PassKey<DeferredShapingMinimumTopScope>,
                            LayoutUnit value) {
    current_minimum_top_ = value;
  }
  // A flag indicating whether the current layout container supports
  // deferred shaping.
  bool AllowDeferredShaping() const { return allow_deferred_shaping_; }
  void SetAllowDeferredShaping(base::PassKey<DeferredShapingDisallowScope>,
                               bool value) {
    allow_deferred_shaping_ = value;
  }
  void SetAllowDeferredShaping(base::PassKey<LocalFrameView>, bool value) {
    allow_deferred_shaping_ = value;
  }

  void PerformPostLayoutTask();
  void RegisterDeferred(Element& element);
  bool IsRegisteredDeferred(Element& element) const;
  void UnregisterDeferred(Element& element);

  // Manage reshaping

  void ReshapeAllDeferred(ReshapeReason reason);
  // Reshape shaping-deferred elements so that |target| can return the precise
  // value of |property_id|.
  // If |property_id| is kInvalid, this function unlocks elements necessary for
  // any geometry of the target node.
  void ReshapeDeferred(ReshapeReason reason,
                       const Node& target,
                       CSSPropertyID property_id = CSSPropertyID::kInvalid);
  // Reshape shaping-deferred elements so that |object| can return the precise
  // width.
  void ReshapeDeferredForWidth(const LayoutObject& object);
  // Reshape shaping-deferred elements so that |object| can return the precise
  // height.
  void ReshapeDeferredForHeight(const LayoutObject& object);
  void OnFirstContentfulPaint();
  void OnResizeFrame();
  void OnFocus(const Element& element);

 private:
  size_t ReshapeAllDeferredInternal();

  TaskHandle reshaping_task_handle_;
  HeapHashSet<Member<Element>> deferred_elements_;
  Member<Document> document_;
  LayoutUnit current_viewport_bottom_ = kIndefiniteSize;
  LayoutUnit current_minimum_top_;
  bool allow_deferred_shaping_ = false;
  bool default_allow_deferred_shaping_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
