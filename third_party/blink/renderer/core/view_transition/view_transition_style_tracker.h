// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_TRACKER_H_

#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/public/common/frame/view_transition_state.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/view_transition_shared_element_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
class PaintLayer;
class PseudoElement;

// This class manages the integration between ViewTransition and the style
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
class ViewTransitionStyleTracker
    : public GarbageCollected<ViewTransitionStyleTracker> {
 public:
  // Properties that transition on container elements.
  struct ContainerProperties {
    ContainerProperties() = default;
    ContainerProperties(const LayoutSize& size, const gfx::Transform& matrix)
        : border_box_size_in_css_space(size), snapshot_matrix(matrix) {}

    bool operator==(const ContainerProperties& other) const {
      return border_box_size_in_css_space ==
                 other.border_box_size_in_css_space &&
             snapshot_matrix == other.snapshot_matrix;
    }
    bool operator!=(const ContainerProperties& other) const {
      return !(*this == other);
    }

    LayoutSize border_box_size_in_css_space;

    // Transforms a point from local space into the snapshot viewport. For
    // details of the snapshot viewport, see README.md.
    gfx::Transform snapshot_matrix;
  };

  explicit ViewTransitionStyleTracker(Document& document);
  ViewTransitionStyleTracker(Document& document, ViewTransitionState);
  ~ViewTransitionStyleTracker();

  void AddSharedElementsFromCSS();

  // Returns true if the pseudo element corresponding to the given id and name
  // is the only child.
  bool MatchForOnlyChild(PseudoId pseudo_id,
                         AtomicString view_transition_name) const;

  // Indicate that capture was requested. This verifies that the combination of
  // set elements and names is valid. Returns true if capture phase started, and
  // false if the transition should be aborted.
  bool Capture();

  // Notifies when caching snapshots for elements in the old DOM finishes. This
  // is dispatched before script is notified to ensure this class releases any
  // references to elements in the old DOM before it is mutated by script.
  void CaptureResolved();

  // Indicate that start was requested. This verifies that the combination of
  // set elements and names is valid. Returns true if start phase started, and
  // false if the transition should be aborted.
  bool Start();

  // Notifies when the animation setup for the transition during Start have
  // finished executing.
  void StartFinished();

  // Dispatched if a transition is aborted. Must be called before "Start" stage
  // is initiated.
  void Abort();

  void UpdateRootIndexAndSnapshotId(
      ViewTransitionElementId&,
      viz::ViewTransitionElementResourceId&) const;

  void UpdateElementIndicesAndSnapshotId(
      Element*,
      ViewTransitionElementId&,
      viz::ViewTransitionElementResourceId&) const;

  // Creates a PseudoElement for the corresponding |pseudo_id| and
  // |view_transition_name|. The |pseudo_id| must be a ::transition* element.
  PseudoElement* CreatePseudoElement(Element* parent,
                                     PseudoId pseudo_id,
                                     const AtomicString& view_transition_name);

  // Dispatched after the pre-paint lifecycle stage after each rendering
  // lifecycle update when a transition is in progress.
  // Returns false if the transition constraints were broken and the transition
  // should be skipped.
  bool RunPostPrePaintSteps();

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

  int CapturedTagCount() const { return captured_name_count_; }

  bool IsSharedElement(Node* node) const;

  // This function represents whether root itself is participating in the
  // transition (i.e. it has a name in the current phase). Note that we create
  // an EffectNode for the root whether or not it's transitioning.
  bool IsRootTransitioning() const;

  std::vector<viz::ViewTransitionElementResourceId> TakeCaptureResourceIds() {
    return std::move(capture_resource_ids_);
  }

  // Returns whether styles applied to pseudo elements should be limited to UA
  // rules based on the current phase of the transition.
  StyleRequest::RulesToInclude StyleRulesToInclude() const;

  VectorOf<Element> GetTransitioningElements() const;

  // In physical pixels. Returns the snapshot viewport rect, relative to the
  // fixed viewport origin. See README.md for a detailed description of the
  // snapshot viewport.
  gfx::Rect GetSnapshotViewportRect() const;

  // In physical pixels. Returns the offset within the root snapshot which
  // should be used as the paint origin. The root snapshot fills the snapshot
  // viewport, which is overlaid by viewport-insetting UI widgets such as the
  // mobile URL bar. Because of this, we offset paint so that content is
  // painted where it appears on the screen (rather than under the UI).
  gfx::Vector2d GetRootSnapshotPaintOffset() const;

  // Returns a serializable representation of the state cached by this class to
  // recreate the same pseudo-element tree in a new Document.
  ViewTransitionState GetViewTransitionState() const;

 private:
  class ImageWrapperPseudoElement;

  // These state transitions are executed in a serial order unless the
  // transition is aborted.
  enum class State { kIdle, kCapturing, kCaptured, kStarted, kFinished };

  struct ElementData : public GarbageCollected<ElementData> {
    void Trace(Visitor* visitor) const;

    // Returns the intrinsic size for the element's snapshot.
    LayoutSize GetIntrinsicSize(bool use_cached_data);

    // Caches the current geometry state for the old snapshot.
    void CacheGeometryState();

    // The element in the current DOM whose state is being tracked and mirrored
    // into the corresponding container pseudo element.
    Member<Element> target_element;

    // Computed info for each element participating in the transition for the
    // |target_element|. This information is mirrored into the UA stylesheet.
    // This is stored in a vector to be able to stack animations.
    Vector<ContainerProperties> container_properties;

    // Computed info cached before the DOM switches to the new state.
    ContainerProperties cached_container_properties;

    // Valid if there is an element in the old DOM generating a snapshot.
    viz::ViewTransitionElementResourceId old_snapshot_id;

    // Valid if there is an element in the new DOM generating a snapshot.
    viz::ViewTransitionElementResourceId new_snapshot_id;

    // An effect used to represent the `target_element`'s contents, including
    // any of element's own effects, in a pseudo element layer.
    scoped_refptr<EffectPaintPropertyNode> effect_node;

    // Index to add to the view transition shared element id.
    int element_index;

    // The visual overflow rect for this element. This is used to compute
    // object-view-box if needed.
    // This rect is in layout space.
    PhysicalRect visual_overflow_rect_in_layout_space;
    PhysicalRect cached_visual_overflow_rect_in_layout_space;

    // The writing mode to use for the container. Note that initially this is
    // the outgoing element's (if any) writing mode, and then switches to the
    // incoming element's writing mode, if one exists.
    WritingMode container_writing_mode = WritingMode::kHorizontalTb;
  };

  struct RootData {
    viz::ViewTransitionElementResourceId snapshot_id;
    VectorOf<AtomicString> names;
  };

  void InvalidateStyle();
  bool HasLiveNewContent() const;
  void EndTransition();

  void AddConsoleError(String message, Vector<DOMNodeId> related_nodes = {});
  void AddSharedElement(Element*, const AtomicString&);
  bool FlattenAndVerifyElements(VectorOf<Element>&,
                                VectorOf<AtomicString>&,
                                absl::optional<RootData>&);

  void AddSharedElementsFromCSSRecursive(PaintLayer*);

  int OldRootDataTagSize() const {
    return old_root_data_ ? old_root_data_->names.size() : 0;
  }
  int NewRootDataTagSize() const {
    return new_root_data_ ? new_root_data_->names.size() : 0;
  }
  absl::optional<RootData> GetCurrentRootData() const;
  HashSet<AtomicString> AllRootTags() const;

  void InvalidateHitTestingCache();

  // Computes the visual overflow rect for the given box. If the ancestor is
  // specified, then the result is mapped to that ancestor space.
  PhysicalRect ComputeVisualOverflowRect(
      LayoutBoxModelObject& box,
      LayoutBoxModelObject* ancestor = nullptr);

  Member<Document> document_;

  // Indicates which step during the transition we're currently at.
  State state_ = State::kIdle;

  // Set if this style tracker was created by deserializing captured state
  // instead of running through the capture phase. This is done for transitions
  // initiated by navigations where capture and animation could run in different
  // Documents which are cross-process.
  const bool deserialized_ = false;

  // Tracks the number of names discovered during the capture phase of the
  // transition.
  int captured_name_count_ = 0;

  // Map of the CSS |view-transition-name| property to state for that tag.
  HeapHashMap<AtomicString, Member<ElementData>> element_data_map_;

  // The device scale factor used for layout of the Document. This is kept in
  // sync with the Document during RunPostPrePaintSteps().
  float device_pixel_ratio_ = 1.f;

  // The data for the |documentElement| generate if it has a valid
  // |view-transition-name| for the old and new DOM state.
  absl::optional<RootData> old_root_data_;
  absl::optional<RootData> new_root_data_;

  // The paint property node for the |documentElement|. This is generated if the
  // element has a valid |view-transition-name| and ensures correct generation
  // of its snapshot.
  scoped_refptr<EffectPaintPropertyNode> root_effect_node_;

  // The dynamically generated UA stylesheet for default styles on
  // pseudo-elements.
  absl::optional<String> ua_style_sheet_;

  // The following state is buffered until the capture phase and populated again
  // by script for the start phase.
  int set_element_sequence_id_ = 0;
  HeapHashMap<Member<Element>, HashSet<std::pair<AtomicString, int>>>
      pending_shared_element_names_;

  // This vector is passed as constructed to cc's view transition request,
  // so this uses the std::vector for that reason, instead of WTF::Vector.
  std::vector<viz::ViewTransitionElementResourceId> capture_resource_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_STYLE_TRACKER_H_
