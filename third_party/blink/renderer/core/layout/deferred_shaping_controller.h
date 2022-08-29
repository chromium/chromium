// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class Document;
class Element;
class LocalFrame;

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
  size_t ReshapeAllDeferred();
  void OnFirstContentfulPaint();

 private:
  void ReshapeAllDeferredInternal();

  Member<LocalFrame> frame_;
  TaskHandle reshaping_task_handle_;
  HeapHashSet<Member<Element>> deferred_elements_;
  bool default_allow_deferred_shaping_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_CONTROLLER_H_
