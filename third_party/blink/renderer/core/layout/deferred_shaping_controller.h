// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class Document;
class Element;
class LayoutObject;
class LocalFrame;
class Node;

enum class ReshapeReason {
  kComputedStyle,
  kDomContentLoaded,  // DOMCntentLoaded after FCP
  kFcp,               // FCP after DOMContentLoaded
  kFragmentAnchor,
  kFocus,
  kGeometryApi,
  kInspector,
  kPrinting,
  kScrollingApi,
};

// DeferredShapingController class manages states of the Deferred Shaping
// feature.
//
// A LocalFrameView owns a DeferredShapingController instance. A LocalFrameView
// and its DeferredShapingController are created and destroyed together.
class CORE_EXPORT DeferredShapingController
    : public GarbageCollected<DeferredShapingController> {
 public:
  // This returns nullptr if the |document| is not active.
  static DeferredShapingController* From(const Document& document);
  explicit DeferredShapingController(LocalFrame& frame);
  void Trace(Visitor* visitor) const;

  // Disable deferred shaping on the frame persistently.
  // This function should not be called during laying out.
  void DisallowDeferredShaping();
  bool DefaultAllowDeferredShaping() const {
    return default_allow_deferred_shaping_;
  }

  void PerformPostLayoutTask();
  void RegisterDeferred(Element& element);
  bool IsRegisteredDeferred(Element& element) const;
  void UnregisterDeferred(Element& element);

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

 private:
  Member<LocalFrame> frame_;
  TaskHandle reshaping_task_handle_;
  HeapHashSet<Member<Element>> deferred_elements_;
  bool default_allow_deferred_shaping_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
