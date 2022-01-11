// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "components/viz/common/shared_element_resource_id.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {
class PseudoElement;

// This class manages the integration between DocumentTransition and the style
// system which encompasses the following responsibilities :
//
// 1) Triggering style invalidation to change the DOM structure at different
//    stages during a transition. For example, pseudo elements for new-content
//    are generated after the new Document has loaded and the transition can be
//    started.
//
// 2) Tracking changes in the state of shared elements that are mirrored in the
//    style for their corresponding pseudo element. For example, if a shared
//    element's size or viewport space transform is updated. This data is used
//    to generate a dynamic UA stylesheet for these pseudo elements.
//
// A new instance of this class is created for every transition.
class DocumentTransitionStyleTracker
    : public GarbageCollected<DocumentTransitionStyleTracker> {
 public:
  explicit DocumentTransitionStyleTracker(Document& document);
  ~DocumentTransitionStyleTracker();

  // Notifies when the transition is initiated. |elements| is the set of shared
  // elements in the old DOM.
  void Prepare(const HeapVector<Member<Element>>& old_elements);

  // Notifies when caching snapshots for elements in the old DOM finishes. This
  // is dispatched before script is notified to ensure this class releases any
  // references to elements in the old DOM before it is mutated by script.
  void PrepareResolved();

  // Notifies when the new DOM has finished loading and a transition can be
  // started. |elements| is the set of shared elements in the new DOM paired
  // sequentially with the list of |elements| in the Prepare call.
  void Start(const HeapVector<Member<Element>>& new_elements);

  // Notifies when the animation setup for the transition during Start have
  // finished executing.
  void StartFinished();

  // Dispatched if a transition is aborted. Must be called before "Start" stage
  // is initiated.
  void Abort();

  // Returns the resource id that |element| should be tagged with. This
  // |element| must be a shared element in the current DOM (specified in Prepare
  // or Start).
  viz::SharedElementResourceId GetLiveSnapshotId(const Element* element) const;

  // Creates a PseudoElement for the corresponding |pseudo_id| and
  // |document_transition_tag|. The |pseudo_id| must be a ::transition* element.
  PseudoElement* CreatePseudoElement(
      Element* parent,
      PseudoId pseudo_id,
      const AtomicString& document_transition_tag);

  // Dispatched after the layout lifecycle stage after each rendering lifecycle
  // update when a transition is in progress.
  void RunPostLayoutSteps();

  // Provides a UA stylesheet applied to ::transition* pseudo elements.
  const String& UAStyleSheet();

  void Trace(Visitor* visitor) const;

 private:
  class ContainerPseudoElement;

  // These state transitions are executed in a serial order unless the
  // transition is aborted.
  enum class State { kIdle, kPreparing, kPrepared, kStarted, kFinished };

  struct ElementData : public GarbageCollected<ElementData> {
    void Trace(Visitor* visitor) const;

    // The element in the current DOM whose state is being tracked and mirrored
    // into the corresponding container pseudo element.
    Member<Element> target_element;

    // Computed info for each element participating in the transition for the
    // |target_element|. This information is mirrored into the UA stylesheet.
    LayoutSize border_box_size;
    TransformationMatrix viewport_matrix;

    // Computed info cached before the DOM switches to the new state.
    LayoutSize cached_border_box_size;
    TransformationMatrix cached_viewport_matrix;

    // Valid if there is an element in the old DOM generating a snapshot.
    viz::SharedElementResourceId old_snapshot_id;

    // Valid if there is an element in the new DOM generating a snapshot.
    viz::SharedElementResourceId new_snapshot_id;
  };

  void InvalidateStyle();
  bool HasLiveNewContent() const;
  void EndTransition();

  Member<Document> document_;
  State state_ = State::kIdle;
  Vector<AtomicString> pseudo_document_transition_tags_;
  HeapHashMap<AtomicString, Member<ElementData>> element_data_map_;
  absl::optional<String> ua_style_sheet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_
