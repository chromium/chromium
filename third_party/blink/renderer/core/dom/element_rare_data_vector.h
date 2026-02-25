// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/element_animation_trigger_data.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/explicitly_set_attr_elements_map.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element_data.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/restriction_target_id.h"
#include "third_party/blink/renderer/platform/tracked_element_id.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

class CSSStyleDeclaration;
class ColumnPseudoElement;
class ContentData;
class ShadowRoot;
class NamedNodeMap;
class DOMTokenList;
class DatasetDOMStringMap;
class DisplayAdElementMonitor;
class ElementAnimations;
class Attr;
typedef HeapVector<Member<Attr>> AttrNodeList;
class ElementIntersectionObserverData;
class ContainerQueryEvaluator;
class EditContext;
class InlineStylePropertyMap;
class ElementInternals;
class DisplayLockContext;
class ContainerQueryData;
class ResizeObserver;
class ResizeObservation;
class StyleScopeData;
class CustomElementDefinition;
class PopoverData;
class InvokerData;
class InterestInvokerTargetData;
class OutOfFlowData;
class HTMLElement;
class Element;
class OverscrollAreaTracker;
enum class DynamicRestyleFlags;
enum class ElementFlags;
class FlatTreeNodeData;
class MutationObserverRegistration;
class NodeListsNodeData;
class Part;
class ScrollTimeline;

enum class ElementFlags;

using TemporaryPartsList = HeapDeque<Member<Part>>;

class NodeMutationObserverData final
    : public GarbageCollected<NodeMutationObserverData>,
      public ElementRareDataField {
 public:
  NodeMutationObserverData() = default;
  NodeMutationObserverData(const NodeMutationObserverData&) = delete;
  NodeMutationObserverData& operator=(const NodeMutationObserverData&) = delete;

  const HeapVector<Member<MutationObserverRegistration>>& Registry() {
    return registry_;
  }

  const HeapHashSet<Member<MutationObserverRegistration>>& TransientRegistry() {
    return transient_registry_;
  }

  void AddTransientRegistration(MutationObserverRegistration* registration);
  void RemoveTransientRegistration(MutationObserverRegistration* registration);
  void AddRegistration(MutationObserverRegistration* registration);
  void RemoveRegistration(MutationObserverRegistration* registration);

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<MutationObserverRegistration>> registry_;
  HeapHashSet<Member<MutationObserverRegistration>> transient_registry_;
};

class ScrollTimelineHashSet final
    : public GarbageCollected<ScrollTimelineHashSet>,
      public ElementRareDataField {
 public:
  HeapHashSet<Member<ScrollTimeline>> set_;

  void Trace(Visitor* visitor) const override;
};

// ElementRareDataVector provides sparse storage of fields for Node and Element
// (most Elements set only 0–3 of the 50+ possible fields). It consists of two
// parts:
//
//  * Fixed bit-fields that are always present (as long as the
//    ElementRareDataVector is allocated at all), and
//  * A variable number of Member<ElementRareDataField>s (“slots”).
//
// The storage scheme for the latter is very similar to SparseVector (a bit set
// saying which values exist, and then the actual Members sorted by ID), but
// stored in AdditionalBytes. This conserves memory and avoids a pointer
// indirection to get to the members (although the actual values are still one
// step away); we don't need to store a pointer, the size can be calculated from
// the bitmap and the capacity is implicit from the size since we never delete
// elements (it is always the size rounded up to the nearest power of two,
// except that there is a minimum size).
//
// However, this means that any Set*() or Ensure*() member function can end up
// having to reallocate the ElementRareDataVector to get more data into
// AdditionalBytes. Thus, every such function will return the new pointer for
// ElementRareDataVector (which may be the same as the previous one, or an
// entirely new object), which means that the caller must update their pointer
// in case it changed. These are all marked by [[nodiscard]] so that you do not
// accidentally forget to do so.
class CORE_EXPORT ElementRareDataVector final
    : public GarbageCollected<ElementRareDataVector> {
 public:
  using PassKey = base::PassKey<ElementRareDataVector>;

  enum {
    kConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
    kNumberOfElementFlags = 8,
    kNumberOfDynamicRestyleFlags = 14
    // 0 bits remaining.
  };

  static ElementRareDataVector* Create() {
    return MakeGarbageCollected<ElementRareDataVector>(
        AdditionalBytes(kMinimumVectorSize * kSlotSizeBytes), PassKey());
  }

  ~ElementRareDataVector();
  ElementRareDataVector(const ElementRareDataVector&) = delete;
  ElementRareDataVector& operator=(const ElementRareDataVector&) = delete;

  void ClearNodeLists() { SetFieldToNullIfExists(FieldId::kNodeLists); }
  NodeListsNodeData* NodeLists() const;
  // EnsureNodeLists() and a following NodeListsNodeData functions must be
  // wrapped with a ThreadState::GCForbiddenScope in order to avoid an
  // initialized node_lists_ is cleared by NodeRareData::TraceAfterDispatch().
  std::pair<std::reference_wrapper<NodeListsNodeData>, ElementRareDataVector*>
  EnsureNodeLists();

  FlatTreeNodeData* GetFlatTreeNodeData() const;
  std::pair<std::reference_wrapper<FlatTreeNodeData>, ElementRareDataVector*>
  EnsureFlatTreeNodeData();

  NodeMutationObserverData* MutationObserverData();
  std::pair<std::reference_wrapper<NodeMutationObserverData>,
            ElementRareDataVector*>
  EnsureMutationObserverData();

  uint16_t ConnectedSubframeCount() const {
    return flags_.connected_frame_count_;
  }
  void IncrementConnectedSubframeCount();
  void DecrementConnectedSubframeCount() {
    DCHECK(flags_.connected_frame_count_);
    --flags_.connected_frame_count_;
  }

  bool HasRestyleFlag(DynamicRestyleFlags mask) const {
    return flags_.restyle_flags_ & static_cast<uint16_t>(mask);
  }
  void SetRestyleFlag(DynamicRestyleFlags mask) {
    flags_.restyle_flags_ |= static_cast<uint16_t>(mask);
  }
  bool HasRestyleFlags() const { return flags_.restyle_flags_; }
  void ClearRestyleFlags() { flags_.restyle_flags_ = 0u; }

  [[nodiscard]] ElementRareDataVector* RegisterScrollTimeline(ScrollTimeline*);
  [[nodiscard]] ElementRareDataVector* UnregisterScrollTimeline(
      ScrollTimeline*);

  // Mostly for accessibility.
  DOMNodeId NodeId() const {
    auto* value = GetWrappedField<DOMNodeId>(FieldId::kDOMNodeId);
    return value ? *value : 0;
  }
  [[nodiscard]] std::pair<std::reference_wrapper<DOMNodeId>,
                          ElementRareDataVector*>
  NodeId() {
    return EnsureWrappedField<DOMNodeId>(FieldId::kDOMNodeId);
  }

  [[nodiscard]] ElementRareDataVector* SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const;
  bool HasScrollButtonOrMarkerGroupPseudos() const;
  PseudoElementData::PseudoElementVector GetPseudoElements() const;

  [[nodiscard]] ElementRareDataVector* AddColumnPseudoElement(
      ColumnPseudoElement&);
  const ColumnPseudoElementsVector* GetColumnPseudoElements() const;
  ColumnPseudoElement* GetColumnPseudoElement(wtf_size_t idx) const;
  void ClearColumnPseudoElements(wtf_size_t to_keep);

  std::pair<std::reference_wrapper<CSSStyleDeclaration>, ElementRareDataVector*>
  EnsureInlineCSSStyleDeclaration(Element* owner_element);

  ShadowRoot* GetShadowRoot() const;
  [[nodiscard]] ElementRareDataVector* SetShadowRoot(ShadowRoot& shadow_root);

  NamedNodeMap* AttributeMap() const;
  [[nodiscard]] ElementRareDataVector* SetAttributeMap(
      NamedNodeMap* attribute_map);

  DOMTokenList* GetClassList() const;
  [[nodiscard]] ElementRareDataVector* SetClassList(DOMTokenList* class_list);

  DatasetDOMStringMap* Dataset() const;
  [[nodiscard]] ElementRareDataVector* SetDataset(DatasetDOMStringMap* dataset);

  ScrollOffset SavedLayerScrollOffset() const;
  [[nodiscard]] ElementRareDataVector* SetSavedLayerScrollOffset(
      ScrollOffset offset);

  ElementAnimations* GetElementAnimations();
  [[nodiscard]] ElementRareDataVector* SetElementAnimations(
      ElementAnimations* element_animations);

  bool HasPseudoElements() const;
  void ClearPseudoElements();

  std::pair<std::reference_wrapper<AttrNodeList>, ElementRareDataVector*>
  EnsureAttrNodeList();
  AttrNodeList* GetAttrNodeList();
  void RemoveAttrNodeList();
  [[nodiscard]] ElementRareDataVector* AddAttr(Attr* attr);

  ElementIntersectionObserverData* IntersectionObserverData() const;
  std::pair<std::reference_wrapper<ElementIntersectionObserverData>,
            ElementRareDataVector*>
  EnsureIntersectionObserverData();

  ContainerQueryEvaluator* GetContainerQueryEvaluator() const;
  [[nodiscard]] ElementRareDataVector* SetContainerQueryEvaluator(
      ContainerQueryEvaluator* evaluator);

  const AtomicString& GetNonce() const;
  [[nodiscard]] ElementRareDataVector* SetNonce(const AtomicString& nonce);

  const AtomicString& IsValue() const;
  [[nodiscard]] ElementRareDataVector* SetIsValue(const AtomicString& is_value);

  EditContext* GetEditContext() const;
  [[nodiscard]] ElementRareDataVector* SetEditContext(
      EditContext* edit_context);

  [[nodiscard]] ElementRareDataVector* SetPart(DOMTokenList* part);
  DOMTokenList* GetPart() const;

  [[nodiscard]] ElementRareDataVector* SetMarker(DOMTokenList* marker);
  DOMTokenList* GetMarker() const;

  [[nodiscard]] ElementRareDataVector* SetPartNamesMap(
      const AtomicString part_names);
  const NamesMap* PartNamesMap() const;

  std::pair<std::reference_wrapper<InlineStylePropertyMap>,
            ElementRareDataVector*>
  EnsureInlineStylePropertyMap(Element* owner_element);
  InlineStylePropertyMap* GetInlineStylePropertyMap();

  const ElementInternals* GetElementInternals() const;
  std::pair<std::reference_wrapper<ElementInternals>, ElementRareDataVector*>
  EnsureElementInternals(HTMLElement& target);

  std::pair<std::reference_wrapper<DisplayLockContext>, ElementRareDataVector*>
  EnsureDisplayLockContext(Element* element);
  DisplayLockContext* GetDisplayLockContext() const;

  std::pair<std::reference_wrapper<ContainerQueryData>, ElementRareDataVector*>
  EnsureContainerQueryData();
  ContainerQueryData* GetContainerQueryData() const;
  void ClearContainerQueryData();

  std::pair<std::reference_wrapper<StyleScopeData>, ElementRareDataVector*>
  EnsureStyleScopeData();
  StyleScopeData* GetStyleScopeData() const;

  std::pair<std::reference_wrapper<OutOfFlowData>, ElementRareDataVector*>
  EnsureOutOfFlowData();
  OutOfFlowData* GetOutOfFlowData() const;
  void ClearOutOfFlowData();

  // Returns the crop-ID if one was set, or nullptr otherwise.
  const RegionCaptureCropId* GetRegionCaptureCropId() const;
  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  [[nodiscard]] ElementRareDataVector* SetRegionCaptureCropId(
      std::unique_ptr<RegionCaptureCropId> crop_id);

  const TrackedElementRect* GetTrackedElementRect() const;
  [[nodiscard]] ElementRareDataVector* SetTrackedElementRect(
      std::unique_ptr<TrackedElementRect> rect);
  void ClearTrackedElementRect();

  // Returns the ID backing a RestrictionTarget if one was set on the Element,
  // or nullptr otherwise.
  const RestrictionTargetId* GetRestrictionTargetId() const;
  // Returns the ID backing a RestrictionTarget if one was set on the Element,
  // or nullptr otherwise.
  // Sets an ID backing a RestrictionTarget associated with the Element.
  // Must be called at most once. Cannot be used to unset a previously set IDs.
  [[nodiscard]] ElementRareDataVector* SetRestrictionTargetId(
      std::unique_ptr<RestrictionTargetId> id);

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;
  ResizeObserverDataMap* ResizeObserverData() const;
  std::pair<std::reference_wrapper<ResizeObserverDataMap>,
            ElementRareDataVector*>
  EnsureResizeObserverData();

  [[nodiscard]] ElementRareDataVector* SetCustomElementDefinition(
      CustomElementDefinition* definition);
  CustomElementDefinition* GetCustomElementDefinition() const;

  [[nodiscard]] ElementRareDataVector* SetLastRememberedBlockSize(
      std::optional<LayoutUnit> size);
  [[nodiscard]] ElementRareDataVector* SetLastRememberedInlineSize(
      std::optional<LayoutUnit> size);
  std::optional<LayoutUnit> LastRememberedBlockSize() const;
  std::optional<LayoutUnit> LastRememberedInlineSize() const;

  PopoverData* GetPopoverData() const;
  std::pair<std::reference_wrapper<PopoverData>, ElementRareDataVector*>
  EnsurePopoverData();
  void RemovePopoverData();

  InvokerData* GetInvokerData() const;
  std::pair<std::reference_wrapper<InvokerData>, ElementRareDataVector*>
  EnsureInvokerData();

  InterestInvokerTargetData* GetInterestInvokerTargetData() const;
  std::pair<std::reference_wrapper<InterestInvokerTargetData>,
            ElementRareDataVector*>
  EnsureInterestInvokerTargetData();
  void RemoveInterestInvokerTargetData();

  bool HasElementFlag(ElementFlags mask) const {
    return flags_.element_flags_ & static_cast<uint16_t>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) {
    flags_.element_flags_ =
        (flags_.element_flags_ & ~static_cast<uint16_t>(mask)) |
        (-static_cast<uint16_t>(value) & static_cast<uint16_t>(mask));
  }
  void ClearElementFlag(ElementFlags mask) {
    flags_.element_flags_ &= ~static_cast<uint16_t>(mask);
  }

  void SetTabIndexExplicitly() {
    SetElementFlag(ElementFlags::kTabIndexWasSetExplicitly, true);
  }
  void ClearTabIndexExplicitly() {
    ClearElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
  }

  ScrollMarkerGroupData* GetScrollMarkerGroupData() const;
  void RemoveScrollMarkerGroupData();
  std::pair<std::reference_wrapper<ScrollMarkerGroupData>,
            ElementRareDataVector*>
  EnsureScrollMarkerGroupData(Element*);

  [[nodiscard]] ElementRareDataVector* SetScrollMarkerGroupContainerData(
      ScrollMarkerGroupData*);
  ScrollMarkerGroupData* GetScrollMarkerGroupContainerData() const;

  [[nodiscard]] ElementRareDataVector* CacheCSSPseudoElement(PseudoId,
                                                             CSSPseudoElement&);
  CSSPseudoElement* GetCSSPseudoElement(PseudoId) const;

  ExplicitlySetAttrElementsMap* GetExplicitlySetElementsForAttr() const;
  std::pair<std::reference_wrapper<ExplicitlySetAttrElementsMap>,
            ElementRareDataVector*>
  EnsureExplicitlySetElementsForAttr();

  AnchorPositionScrollData* GetAnchorPositionScrollData() const;
  void RemoveAnchorPositionScrollData();
  std::pair<std::reference_wrapper<AnchorPositionScrollData>,
            ElementRareDataVector*>
  EnsureAnchorPositionScrollData(Element*);

  bool HasCustomElementRegistrySet() const;
  CustomElementRegistry* GetCustomElementRegistry() const;
  [[nodiscard]] ElementRareDataVector* SetCustomElementRegistry(
      CustomElementRegistry* registry);
  void ClearCustomElementRegistry();

  ElementAnimationTriggerData* AnimationTriggerData();
  std::pair<std::reference_wrapper<ElementAnimationTriggerData>,
            ElementRareDataVector*>
  EnsureAnimationTriggerData();

  DisplayAdElementMonitor* GetDisplayAdElementMonitor() const;
  std::pair<std::reference_wrapper<DisplayAdElementMonitor>,
            ElementRareDataVector*>
  EnsureDisplayAdElementMonitor(Element*);

  void SetDidAttachInternals() { flags_.did_attach_internals = true; }
  bool DidAttachInternals() const { return flags_.did_attach_internals; }
  bool HasUndoStack() const { return flags_.has_undo_stack; }
  void SetHasUndoStack(bool value) { flags_.has_undo_stack = value; }
  void SetPseudoElementStylesChangeCounters(bool value) {
    flags_.has_counters_styles = value;
  }
  bool PseudoElementStylesAffectCounters() const {
    return flags_.has_counters_styles;
  }
  bool ScrollbarPseudoElementStylesDependOnFontMetrics() const {
    return flags_.scrollbar_pseudo_element_styles_depend_on_font_metrics;
  }
  void SetScrollbarPseudoElementStylesDependOnFontMetrics(bool value) {
    flags_.scrollbar_pseudo_element_styles_depend_on_font_metrics = value;
  }
  void SetHasBeenExplicitlyScrolled() {
    flags_.has_been_explicitly_scrolled = true;
  }
  bool HasBeenExplicitlyScrolled() const {
    return flags_.has_been_explicitly_scrolled;
  }
  bool MayBeImplicitAnchor() const { return flags_.may_be_implicit_anchor; }
  void SetMayBeImplicitAnchor() { flags_.may_be_implicit_anchor = true; }

  FocusgroupData GetFocusgroupData() const {
    return {static_cast<FocusgroupBehavior>(flags_.focusgroup_behavior_),
            static_cast<FocusgroupFlags>(flags_.focusgroup_flags_)};
  }
  void SetFocusgroupData(FocusgroupData data) {
    flags_.focusgroup_behavior_ = static_cast<unsigned>(data.behavior);
    flags_.focusgroup_flags_ = static_cast<unsigned>(data.flags);
    DCHECK_EQ(GetFocusgroupData().behavior, data.behavior);
    DCHECK_EQ(GetFocusgroupData().flags, data.flags);
  }
  void ClearFocusgroupData() {
    flags_.focusgroup_behavior_ =
        static_cast<unsigned>(FocusgroupBehavior::kNoBehavior);
    flags_.focusgroup_flags_ = static_cast<unsigned>(FocusgroupFlags::kNone);
    SetFieldToNullIfExists(FieldId::kFocusgroupLastFocused);
  }
  [[nodiscard]] ElementRareDataVector* SetFocusgroupLastFocused(
      Element* element);
  Element* GetFocusgroupLastFocused() const;
  void ClearFocusgroupLastFocused() {
    SetFieldToNullIfExists(FieldId::kFocusgroupLastFocused);
  }

  [[nodiscard]] ElementRareDataVector* SetOverscrollContainer(Element* element);
  Element* GetOverscrollContainer() const;
  void ClearOverscrollContainer() {
    SetFieldToNullIfExists(FieldId::kOverscrollContainer);
  }

  void SetAffectedByStartingStyles() {
    flags_.affected_by_starting_styles = true;
  }
  bool AffectedByStartingStyles() const {
    return flags_.affected_by_starting_styles;
  }
  bool AffectedBySubjectHas() const { return flags_.affected_by_subject_has_; }
  void SetAffectedBySubjectHas() { flags_.affected_by_subject_has_ = true; }
  bool AffectedByNonSubjectHas() const {
    return flags_.affected_by_non_subject_has_;
  }
  void SetAffectedByNonSubjectHas() {
    flags_.affected_by_non_subject_has_ = true;
  }
  bool AncestorsOrAncestorSiblingsAffectedByHas() const {
    return flags_.ancestors_or_ancestor_siblings_affected_by_has_;
  }
  void SetAncestorsOrAncestorSiblingsAffectedByHas() {
    flags_.ancestors_or_ancestor_siblings_affected_by_has_ = true;
  }
  unsigned GetSiblingsAffectedByHasFlags() const {
    return flags_.siblings_affected_by_has_;
  }
  bool HasSiblingsAffectedByHasFlags(unsigned flags) const {
    return flags_.siblings_affected_by_has_ & flags;
  }
  void SetSiblingsAffectedByHasFlags(unsigned flags) {
    flags_.siblings_affected_by_has_ |= flags;
  }
  bool AffectedByPseudoInHas() const {
    return flags_.affected_by_pseudos_in_has_;
  }
  void SetAffectedByPseudoInHas() { flags_.affected_by_pseudos_in_has_ = true; }
  bool AncestorsOrSiblingsAffectedByHoverInHas() const {
    return flags_.ancestors_or_siblings_affected_by_hover_in_has_;
  }
  void SetAncestorsOrSiblingsAffectedByHoverInHas() {
    flags_.ancestors_or_siblings_affected_by_hover_in_has_ = true;
  }
  bool AncestorsOrSiblingsAffectedByActiveInHas() const {
    return flags_.ancestors_or_siblings_affected_by_active_in_has_;
  }
  void SetAncestorsOrSiblingsAffectedByActiveInHas() {
    flags_.ancestors_or_siblings_affected_by_active_in_has_ = true;
  }
  bool AncestorsOrSiblingsAffectedByFocusInHas() const {
    return flags_.ancestors_or_siblings_affected_by_focus_in_has_;
  }
  void SetAncestorsOrSiblingsAffectedByFocusInHas() {
    flags_.ancestors_or_siblings_affected_by_focus_in_has_ = true;
  }
  bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const {
    return flags_.ancestors_or_siblings_affected_by_focus_visible_in_has_;
  }
  void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() {
    flags_.ancestors_or_siblings_affected_by_focus_visible_in_has_ = true;
  }
  bool AffectedByLogicalCombinationsInHas() const {
    return flags_.affected_by_logical_combinations_in_has_;
  }
  void SetAffectedByLogicalCombinationsInHas() {
    flags_.affected_by_logical_combinations_in_has_ = true;
  }
  bool AffectedByMultipleHas() const {
    return flags_.affected_by_multiple_has_;
  }
  void SetAffectedByMultipleHas() { flags_.affected_by_multiple_has_ = true; }

  ContentData* GetAltContentData() const;
  [[nodiscard]] ElementRareDataVector* SetAltContentData(
      ContentData* content_data);

  bool WasLastFocusFromUserGesture() const {
    return flags_.was_last_focus_from_user_gesture;
  }
  void SetWasLastFocusFromUserGesture(bool value) {
    flags_.was_last_focus_from_user_gesture = value;
  }

  std::pair<std::reference_wrapper<OverscrollAreaTracker>,
            ElementRareDataVector*>
  EnsureOverscrollAreaTracker(Element*);
  OverscrollAreaTracker* OverscrollAreaTracker() const;

  void Trace(Visitor*) const;

  explicit ElementRareDataVector(PassKey) {}
  ElementRareDataVector(PassKey, ElementRareDataVector&& other)
      : flags_(other.flags_), fields_bitfield_(other.fields_bitfield_.load()) {
    UNSAFE_BUFFERS(
        VectorTypeOperations<Member<ElementRareDataField>, HeapAllocator>::Move(
            other.ArrayBase(), other.ArrayBase() + other.size(), ArrayBase(),
            VectorOperationOrigin::kConstruction));

#if DCHECK_IS_ON()
    // Clear out the Members so that a) they are not inadvertently
    // used on errors, and b) the DCHECK in the destructor does not fire.
    for (unsigned i = 0; i < other.size(); ++i) {
      UNSAFE_BUFFERS(other.ArrayBase()[i] = nullptr);
    }
#endif
  }

 private:
  friend class ElementRareDataVectorTest;
  enum class FieldId : unsigned {
    kDataset = 0,
    kShadowRoot = 1,
    kClassList = 2,
    kAttributeMap = 3,
    kAttrNodeList = 4,
    kCssomWrapper = 5,
    kElementAnimations = 6,
    kIntersectionObserverData = 7,
    kPseudoElementData = 8,
    kEditContext = 9,
    kPart = 10,
    kCssomMapWrapper = 11,
    kElementInternals = 12,
    kDisplayLockContext = 13,
    kContainerQueryData = 14,
    kRegionCaptureCropId = 15,
    kResizeObserverData = 16,
    kCustomElementDefinition = 17,
    kPopoverData = 18,
    kPartNamesMap = 19,
    kNonce = 20,
    kIsValue = 21,
    kSavedLayerScrollOffset = 22,
    kAnchorPositionScrollData = 23,
    kMayBeImplicitAnchor = 24,
    kLastRememberedBlockSize = 25,
    kLastRememberedInlineSize = 26,
    kRestrictionTargetId = 27,
    kStyleScopeData = 28,
    kOutOfFlowData = 29,
    kInvokerData = 30,
    kInterestInvokerTargetData = 31,
    kScrollMarkerGroupData = 32,
    kScrollMarkerGroupContainerData = 33,
    kExplicitlySetElementsForAttr = 34,
    kCSSPseudoElementData = 35,
    kCustomElementRegistry = 36,
    kAnimationTriggerData = 37,
    kFocusgroupLastFocused = 38,
    kDisplayAdElementMonitor = 39,
    kOverscrollAreaTracker = 40,
    kAltContentData = 41,
    kOverscrollContainer = 42,
    kTrackedElementRect = 43,
    kNodeLists = 44,
    kMutationObserverData = 45,
    kFlatTreeNodeData = 46,
    kScrollTimelines = 47,
    kDOMNodeId = 49,
    kMarker = 50,
    kNumFields = 51,
  };

  inline const Member<ElementRareDataField>* ArrayBase() const {
    static_assert(sizeof(*this) % alignof(Member<ElementRareDataField>) == 0,
                  "ValueArray may be improperly aligned");
    // SAFETY: By funneling all allocation of ElementRareDataVector through
    // Create(), we guarantee that the array will exist where we expect it.
    return UNSAFE_BUFFERS(
        reinterpret_cast<const Member<ElementRareDataField>*>(this + 1));
  }
  inline Member<ElementRareDataField>* ArrayBase() {
    return const_cast<Member<ElementRareDataField>*>(
        const_cast<const ElementRareDataVector*>(this)->ArrayBase());
  }

  const Member<ElementRareDataField>& ArraySlot(FieldId field_id) const {
    // SAFETY: Every modification of fields_bitfield_ goes through
    // SetField(), which makes sure there's always enough room
    // in AdditionalBytes.
    return UNSAFE_BUFFERS(ArrayBase()[GetFieldIndex(field_id)]);
  }

  Member<ElementRareDataField>& ArraySlot(FieldId field_id) {
    // SAFETY: Every modification of fields_bitfield_ goes through
    // SetField(), which makes sure there's always enough room
    // in AdditionalBytes.
    return UNSAFE_BUFFERS(ArrayBase()[GetFieldIndex(field_id)]);
  }

  ElementRareDataField* GetField(FieldId field_id) const;
  [[nodiscard]] ElementRareDataVector* SetField(FieldId field_id,
                                                ElementRareDataField* field);
  void SetFieldToNullIfExists(FieldId field_id);

  template <typename T>
  class DataFieldWrapper final : public GarbageCollected<DataFieldWrapper<T>>,
                                 public ElementRareDataField {
   public:
    T& Get() { return data_; }
    void Trace(Visitor* visitor) const override {
      ElementRareDataField::Trace(visitor);
      TraceIfNeeded<T>::Trace(visitor, data_);
    }

   private:
    T data_;
  };

  template <typename T, typename... Args>
  [[nodiscard]] std::pair<std::reference_wrapper<T>, ElementRareDataVector*>
  EnsureField(FieldId field_id, Args&&... args) {
    T* field = static_cast<T*>(GetField(field_id));
    ElementRareDataVector* vec = this;
    if (!field) {
      field = MakeGarbageCollected<T>(std::forward<Args>(args)...);
      vec = SetField(field_id, field);
    }
    return {*field, vec};
  }

  template <typename T>
  [[nodiscard]] std::pair<std::reference_wrapper<T>, ElementRareDataVector*>
  EnsureWrappedField(FieldId field_id) {
    auto [field, vec] = EnsureField<DataFieldWrapper<T>>(field_id);
    return {field.get().Get(), vec};
  }

  template <typename T, typename U>
  [[nodiscard]] ElementRareDataVector* SetWrappedField(FieldId field_id,
                                                       U data) {
    auto [field, vec] = EnsureField<DataFieldWrapper<T>>(field_id);
    field.get().Get() = std::move(data);
    return vec;
  }

  template <typename T>
  T* GetWrappedField(FieldId field_id) const {
    auto* wrapper = static_cast<DataFieldWrapper<T>*>(GetField(field_id));
    return wrapper ? &wrapper->Get() : nullptr;
  }

  template <typename T>
  [[nodiscard]] ElementRareDataVector* SetOptionalField(FieldId field_id,
                                                        std::optional<T> data) {
    if (data) {
      return SetWrappedField<T>(field_id, *data);
    } else {
      return SetField(field_id, nullptr);
    }
  }

  template <typename T>
  std::optional<T> GetOptionalField(FieldId field_id) const {
    if (auto* value = GetWrappedField<T>(field_id)) {
      return *value;
    }
    return std::nullopt;
  }

 private:
  using BitfieldType = uint64_t;
  static constexpr size_t kMaxSize = 64;

  using Slot = Member<ElementRareDataVector>;
  static constexpr size_t kSlotSizeBytes = sizeof(Slot);

  // Most RareData vectors seem to have at least one element,
  // and due to Oilpan alignment, there's no point in having
  // an odd number of elements, so 2 is a reasonable default.
  //
  // This must be a power of two.
  static constexpr unsigned kMinimumVectorSize = 2;

  wtf_size_t size() const { return std::popcount(fields_bitfield_.load()); }

  // Returns whether the field exists. Time complexity is O(1).
  bool HasField(FieldId field_id) const {
    return fields_bitfield_ & FieldIdMask(field_id);
  }

  static BitfieldType FieldIdMask(FieldId field_id) {
    CHECK_LT(static_cast<wtf_size_t>(field_id), kMaxSize);
    return static_cast<BitfieldType>(1) << static_cast<wtf_size_t>(field_id);
  }

  // Returns a mask that has entries for all field IDs lower than `upper`.
  static BitfieldType LowerFieldIdsMask(FieldId upper) {
    return FieldIdMask(upper) - 1;
  }

  // Returns the index in `fields_` that `field_id` is stored in. If `fields_`
  // isn't storing a field for `field_id`, then this returns the index which
  // the data for `field_id` should be inserted into.
  wtf_size_t GetFieldIndex(FieldId field_id) const {
    // Then count the total population of field IDs lower than that one we
    // are looking for. The target field ID should be located at the index of
    // of the total population.
    return std::popcount(fields_bitfield_ & LowerFieldIdsMask(field_id));
  }

  struct Flags {
    uint32_t restyle_flags_ : kNumberOfDynamicRestyleFlags = 0u;
    uint32_t connected_frame_count_ : kConnectedFrameCountBits = 0u;
    uint32_t element_flags_ : kNumberOfElementFlags = 0u;

    unsigned did_attach_internals : 1 = false;
    unsigned has_undo_stack : 1 = false;
    unsigned scrollbar_pseudo_element_styles_depend_on_font_metrics : 1 = false;
    // This never gets reset, since we would have to keep track for
    // every pseudo-element whether it has counter style or not.
    // But since situations when counter style if removed from
    // pseudo-element are rare, we are fine with it, since
    // it doesn't hurt performance much.
    unsigned has_counters_styles : 1 = false;
    unsigned has_been_explicitly_scrolled : 1 = false;
    unsigned may_be_implicit_anchor : 1 = false;
    unsigned affected_by_starting_styles : 1 = false;
    // This records the last type of a focus on this element via `SetFocused`
    // (or more accurately, the only derived value we need from that).
    // For more see:
    // https://explainers-by-googlers.github.io/user-dictionary-leaks/
    unsigned was_last_focus_from_user_gesture : 1 = false;

    // :has() invalidation flags. See has_invalidation_flags.h for description.
    unsigned affected_by_subject_has_ : 1 = false;
    unsigned affected_by_non_subject_has_ : 1 = false;
    unsigned affected_by_pseudos_in_has_ : 1 = false;
    unsigned siblings_affected_by_has_ : 2 = 0;
    unsigned ancestors_or_ancestor_siblings_affected_by_has_ : 1 = false;
    unsigned ancestors_or_siblings_affected_by_hover_in_has_ : 1 = false;
    unsigned ancestors_or_siblings_affected_by_active_in_has_ : 1 = false;
    unsigned ancestors_or_siblings_affected_by_focus_in_has_ : 1 = false;
    unsigned ancestors_or_siblings_affected_by_focus_visible_in_has_ : 1 =
        false;
    unsigned affected_by_logical_combinations_in_has_ : 1 = false;
    unsigned affected_by_multiple_has_ : 1 = false;

    // Underlying type is FocusgroupBehavior.
    unsigned focusgroup_behavior_ : 4 =
        static_cast<unsigned>(FocusgroupBehavior::kNoBehavior);

    // Underlying type is FocusgroupFlags.
    unsigned focusgroup_flags_ : 7 =
        static_cast<unsigned>(FocusgroupFlags::kNone);

    // We need to be able to distinguish between unset CustomElementRegistry
    // and explicitly nullptr CustomElementRegistry.
    unsigned has_custom_element_registry_ : 1 = false;

    // Currently no free bits left.
  };

  Flags flags_;

  // Atomic because we might get traced from another thread.
  std::atomic<BitfieldType> fields_bitfield_ = 0;
};

template <>
struct ThreadingTrait<blink::ElementRareDataVector> {
  static constexpr ThreadAffinity kAffinity = kMainThreadOnly;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
