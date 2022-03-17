// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_

#include "components/viz/common/shared_element_resource_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/document_transition_shared_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
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

  void AddSharedElement(Element*, const AtomicString&);
  void RemoveSharedElement(Element*);

  // Indicate that capture was requested. This verifies that the combination of
  // set elements and tags is valid. Returns true if capture phase started, and
  // false if the transition should be aborted.
  bool Capture();

  // Notifies when caching snapshots for elements in the old DOM finishes. This
  // is dispatched before script is notified to ensure this class releases any
  // references to elements in the old DOM before it is mutated by script.
  void CaptureResolved();

  // Indicate that start was requested. This verifies that the combination of
  // set elements and tags is valid. Returns true if start phase started, and
  // false if the transition should be aborted.
  bool Start();

  // Notifies when the animation setup for the transition during Start have
  // finished executing.
  void StartFinished();

  // Dispatched if a transition is aborted. Must be called before "Start" stage
  // is initiated.
  void Abort();

  void UpdateRootIndexAndSnapshotId(DocumentTransitionSharedElementId&,
                                    viz::SharedElementResourceId&) const;

  void UpdateElementIndicesAndSnapshotId(Element*,
                                         DocumentTransitionSharedElementId&,
                                         viz::SharedElementResourceId&) const;

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

  // Returns true if any of the pseudo elements are currently participating in
  // an animation.
  bool HasActiveAnimations() const;

  // Updates an effect node with the given state. The return value is a result
  // of updating the effect node.
  PaintPropertyChangeType UpdateEffect(
      Element* element,
      EffectPaintPropertyNode::State state,
      const EffectPaintPropertyNodeOrAlias& current_effect);
  PaintPropertyChangeType UpdateRootEffect(
      EffectPaintPropertyNode::State state,
      const EffectPaintPropertyNodeOrAlias& current_effect);

  EffectPaintPropertyNode* GetEffect(Element* element) const;
  EffectPaintPropertyNode* GetRootEffect() const;

  void VerifySharedElements();

  int CapturedTagCount() const { return captured_tag_count_; }

  bool IsSharedElement(Element* element) const;

  std::vector<viz::SharedElementResourceId> TakeCaptureResourceIds() {
    return std::move(capture_resource_ids_);
  }

  static bool IsReservedTransitionTag(const StringView& value);

 private:
  class ImageWrapperPseudoElement;

  // These state transitions are executed in a serial order unless the
  // transition is aborted.
  enum class State { kIdle, kCapturing, kCaptured, kStarted, kFinished };

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

    // An effect used to represent the `target_element`'s contents, including
    // any of element's own effects, in a pseudo element layer.
    scoped_refptr<EffectPaintPropertyNode> effect_node;

    // Index to add to the document transition shared element id.
    int element_index;
  };

  void InvalidateStyle();
  bool HasLiveNewContent() const;
  void EndTransition();

  void AddConsoleError(String message, Vector<DOMNodeId> related_nodes = {});
  bool FlattenAndVerifyElements(VectorOf<Element>&, VectorOf<AtomicString>&);

  Member<Document> document_;
  State state_ = State::kIdle;
  int captured_tag_count_ = 0;
  HeapHashMap<AtomicString, Member<ElementData>> element_data_map_;
  viz::SharedElementResourceId old_root_snapshot_id_;
  viz::SharedElementResourceId new_root_snapshot_id_;
  scoped_refptr<EffectPaintPropertyNode> root_effect_node_;
  absl::optional<String> ua_style_sheet_;

  // The following state is buffered until the capture phase and populated again
  // by script for the start phase.
  int set_element_sequence_id_ = 0;
  HeapHashMap<Member<Element>, HashSet<std::pair<AtomicString, int>>>
      pending_shared_element_tags_;

  // This vector is passed as constructed to cc's document transition request,
  // so this uses the std::vector for that reason, instead of WTF::Vector.
  std::vector<viz::SharedElementResourceId> capture_resource_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_STYLE_TRACKER_H_
