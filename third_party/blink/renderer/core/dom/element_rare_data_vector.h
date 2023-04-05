// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element_data.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSStyleDeclaration;
class ShadowRoot;
class NamedNodeMap;
class DOMTokenList;
class DatasetDOMStringMap;
class ElementAnimations;
class Attr;
typedef HeapVector<Member<Attr>> AttrNodeList;
class ElementIntersectionObserverData;
class ContainerQueryEvaluator;
class EditContext;
class AnchorElementObserver;
class InlineStylePropertyMap;
class ElementInternals;
class AccessibleNode;
class DisplayLockContext;
class ContainerQueryData;
class ResizeObserver;
class ResizeObservation;
class CustomElementDefinition;
class PopoverData;
class CSSToggleMap;
class HTMLElement;

enum class ElementFlags;

// This class stores lazily-initialized state associated with Elements, each of
// which is identified in the FieldId enum. Since storing pointers to all of
// these classes would take up too much memory, we use a Vector and only include
// the types that have actually been requested. In order to determine which
// index into the vector each type has, an additional bitfield is used to
// indicate which types are currently included in the vector.
//
// Here is an example of what the vector and bitfield would look like if this
// class has initialized a ShadowRoot and an EditContext. We can figure out that
// the first item in the vector is a ShadowRoot because ShadowRoot's spot in the
// bitfield is 1 and everything to the right is a 0. We can figure out that the
// second item is an EditContext because EditContext's spot in the bitfield is a
// 1 and there is one 1 in all of the bits to the right.
// Vector:
//   0: Member<ShadowRoot>
//   1: Member<EditContext>
// Bitfield: 0b00000000000000000000001000000010
class CORE_EXPORT ElementRareDataVector final : public NodeRareData {
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
    kAccessibleNode = 13,
    kDisplayLockContext = 14,
    kContainerQueryData = 15,
    kRegionCaptureCropId = 16,
    kResizeObserverData = 17,
    kCustomElementDefinition = 18,
    kPopoverData = 19,
    kToggleMap = 20,
    kPartNamesMap = 21,
    kNonce = 22,
    kIsValue = 23,
    kSavedLayerScrollOffset = 24,
    kAnchorScrollData = 25,
    kAnchorElementObserver = 26,
    kImplicitlyAnchoredElementCount = 27,
    kLastRememberedBlockSize = 28,
    kLastRememberedInlineSize = 29,

    kNumFields = 30,
  };

  ElementRareDataField* GetField(FieldId field_id) const;
  // GetFieldIndex returns the index in |fields_| that |field_id| is stored in.
  // If |fields_| isn't storing a field for |field_id|, then this returns the
  // index which the data for |field_id| should be inserted into.
  unsigned GetFieldIndex(FieldId field_id) const;
  void SetField(FieldId field_id, ElementRareDataField* field);

  HeapVector<Member<ElementRareDataField>> fields_;
  using BitfieldType = uint32_t;
  BitfieldType fields_bitfield_;
  static_assert(sizeof(fields_bitfield_) * 8 >=
                    static_cast<unsigned>(FieldId::kNumFields),
                "field_bitfield_ must be big enough to have a bit for each "
                "field in FieldId.");

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
    GC_PLUGIN_IGNORE("Why is std::unique_ptr failing? http://crbug.com/1395024")
    T data_;
  };

  template <typename T, typename... Args>
  T& EnsureField(FieldId field_id, Args&&... args) {
    T* field = static_cast<T*>(GetField(field_id));
    if (!field) {
      field = MakeGarbageCollected<T>(std::forward<Args>(args)...);
      SetField(field_id, field);
    }
    return *field;
  }

  template <typename T>
  T& EnsureWrappedField(FieldId field_id) {
    return EnsureField<DataFieldWrapper<T>>(field_id).Get();
  }

  template <typename T, typename U>
  void SetWrappedField(FieldId field_id, U data) {
    EnsureWrappedField<T>(field_id) = std::move(data);
  }

  template <typename T>
  T* GetWrappedField(FieldId field_id) const {
    auto* wrapper = static_cast<DataFieldWrapper<T>*>(GetField(field_id));
    return wrapper ? &wrapper->Get() : nullptr;
  }

  template <typename T>
  void SetOptionalField(FieldId field_id, absl::optional<T> data) {
    if (data) {
      SetWrappedField<T>(field_id, *data);
    } else {
      SetField(field_id, nullptr);
    }
  }

  template <typename T>
  absl::optional<T> GetOptionalField(FieldId field_id) const {
    if (auto* value = GetWrappedField<T>(field_id)) {
      return *value;
    }
    return absl::nullopt;
  }

 public:
  explicit ElementRareDataVector(NodeData*);
  ~ElementRareDataVector() override;

  void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const;
  PseudoElementData::PseudoElementVector GetPseudoElements() const;

  CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(Element* owner_element);

  ShadowRoot* GetShadowRoot() const;
  void SetShadowRoot(ShadowRoot& shadow_root);

  NamedNodeMap* AttributeMap() const;
  void SetAttributeMap(NamedNodeMap* attribute_map);

  DOMTokenList* GetClassList() const;
  void SetClassList(DOMTokenList* class_list);

  DatasetDOMStringMap* Dataset() const;
  void SetDataset(DatasetDOMStringMap* dataset);

  ScrollOffset SavedLayerScrollOffset() const;
  void SetSavedLayerScrollOffset(ScrollOffset offset);

  ElementAnimations* GetElementAnimations();
  void SetElementAnimations(ElementAnimations* element_animations);

  bool HasPseudoElements() const;
  void ClearPseudoElements();

  AttrNodeList& EnsureAttrNodeList();
  AttrNodeList* GetAttrNodeList();
  void RemoveAttrNodeList();
  void AddAttr(Attr* attr);

  ElementIntersectionObserverData* IntersectionObserverData() const;
  ElementIntersectionObserverData& EnsureIntersectionObserverData();

  ContainerQueryEvaluator* GetContainerQueryEvaluator() const;
  void SetContainerQueryEvaluator(ContainerQueryEvaluator* evaluator);

  const AtomicString& GetNonce() const;
  void SetNonce(const AtomicString& nonce);

  const AtomicString& IsValue() const;
  void SetIsValue(const AtomicString& is_value);

  EditContext* GetEditContext() const;
  void SetEditContext(EditContext* edit_context);

  void SetPart(DOMTokenList* part);
  DOMTokenList* GetPart() const;

  void SetPartNamesMap(const AtomicString part_names);
  const NamesMap* PartNamesMap() const;

  InlineStylePropertyMap& EnsureInlineStylePropertyMap(Element* owner_element);
  InlineStylePropertyMap* GetInlineStylePropertyMap();

  const ElementInternals* GetElementInternals() const;
  ElementInternals& EnsureElementInternals(HTMLElement& target);

  AccessibleNode* GetAccessibleNode() const;
  AccessibleNode* EnsureAccessibleNode(Element* owner_element);
  void ClearAccessibleNode();

  DisplayLockContext* EnsureDisplayLockContext(Element* element);
  DisplayLockContext* GetDisplayLockContext() const;

  ContainerQueryData& EnsureContainerQueryData();
  ContainerQueryData* GetContainerQueryData() const;
  void ClearContainerQueryData();

  // Returns the crop-ID if one was set, or nullptr otherwise.
  const RegionCaptureCropId* GetRegionCaptureCropId() const;
  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  void SetRegionCaptureCropId(std::unique_ptr<RegionCaptureCropId> crop_id);

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;
  ResizeObserverDataMap* ResizeObserverData() const;
  ResizeObserverDataMap& EnsureResizeObserverData();

  void SetCustomElementDefinition(CustomElementDefinition* definition);
  CustomElementDefinition* GetCustomElementDefinition() const;

  void SetLastRememberedBlockSize(absl::optional<LayoutUnit> size);
  void SetLastRememberedInlineSize(absl::optional<LayoutUnit> size);
  absl::optional<LayoutUnit> LastRememberedBlockSize() const;
  absl::optional<LayoutUnit> LastRememberedInlineSize() const;

  PopoverData* GetPopoverData() const;
  PopoverData& EnsurePopoverData();
  void RemovePopoverData();

  CSSToggleMap* GetToggleMap() const;
  CSSToggleMap& EnsureToggleMap(Element* owner_element);

  bool HasElementFlag(ElementFlags mask) const {
    return element_flags_ & static_cast<uint16_t>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) {
    element_flags_ =
        (element_flags_ & ~static_cast<uint16_t>(mask)) |
        (-static_cast<uint16_t>(value) & static_cast<uint16_t>(mask));
  }
  void ClearElementFlag(ElementFlags mask) {
    element_flags_ &= ~static_cast<uint16_t>(mask);
  }

  bool HasRestyleFlags() const { return bit_field_.get<RestyleFlags>(); }
  void ClearRestyleFlags() { bit_field_.set<RestyleFlags>(0); }

  void SetTabIndexExplicitly() {
    SetElementFlag(ElementFlags::kTabIndexWasSetExplicitly, true);
  }
  void ClearTabIndexExplicitly() {
    ClearElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
  }

  AnchorScrollData* GetAnchorScrollData() const;
  void RemoveAnchorScrollData();
  AnchorScrollData& EnsureAnchorScrollData(Element*);

  AnchorElementObserver& EnsureAnchorElementObserver(HTMLElement*);
  AnchorElementObserver* GetAnchorElementObserver() const;

  void IncrementImplicitlyAnchoredElementCount();
  void DecrementImplicitlyAnchoredElementCount();
  bool HasImplicitlyAnchoredElement() const;

  void SetDidAttachInternals() { did_attach_internals_ = true; }
  bool DidAttachInternals() const { return did_attach_internals_; }
  bool HasUndoStack() const { return has_undo_stack_; }
  void SetHasUndoStack(bool value) { has_undo_stack_ = value; }
  bool ScrollbarPseudoElementStylesDependOnFontMetrics() const {
    return scrollbar_pseudo_element_styles_depend_on_font_metrics_;
  }
  void SetScrollbarPseudoElementStylesDependOnFontMetrics(bool value) {
    scrollbar_pseudo_element_styles_depend_on_font_metrics_ = value;
  }

  FocusgroupFlags GetFocusgroupFlags() const { return focusgroup_flags_; }
  void SetFocusgroupFlags(FocusgroupFlags flags) { focusgroup_flags_ = flags; }
  void ClearFocusgroupFlags() { focusgroup_flags_ = FocusgroupFlags::kNone; }

  bool AffectedBySubjectHas() const {
    return has_invalidation_flags_.affected_by_subject_has;
  }
  void SetAffectedBySubjectHas() {
    has_invalidation_flags_.affected_by_subject_has = true;
  }
  bool AffectedByNonSubjectHas() const {
    return has_invalidation_flags_.affected_by_non_subject_has;
  }
  void SetAffectedByNonSubjectHas() {
    has_invalidation_flags_.affected_by_non_subject_has = true;
  }
  bool AncestorsOrAncestorSiblingsAffectedByHas() const {
    return has_invalidation_flags_
        .ancestors_or_ancestor_siblings_affected_by_has;
  }
  void SetAncestorsOrAncestorSiblingsAffectedByHas() {
    has_invalidation_flags_.ancestors_or_ancestor_siblings_affected_by_has =
        true;
  }
  unsigned GetSiblingsAffectedByHasFlags() const {
    return has_invalidation_flags_.siblings_affected_by_has;
  }
  bool HasSiblingsAffectedByHasFlags(unsigned flags) const {
    return has_invalidation_flags_.siblings_affected_by_has & flags;
  }
  void SetSiblingsAffectedByHasFlags(unsigned flags) {
    has_invalidation_flags_.siblings_affected_by_has |= flags;
  }
  bool AffectedByPseudoInHas() const {
    return has_invalidation_flags_.affected_by_pseudos_in_has;
  }
  void SetAffectedByPseudoInHas() {
    has_invalidation_flags_.affected_by_pseudos_in_has = true;
  }
  bool AncestorsOrSiblingsAffectedByHoverInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_hover_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByHoverInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_hover_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByActiveInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_active_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByActiveInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_active_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByFocusInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByFocusInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_focus_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_visible_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() {
    has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_visible_in_has = true;
  }
  bool AffectedByLogicalCombinationsInHas() const {
    return has_invalidation_flags_.affected_by_logical_combinations_in_has;
  }
  void SetAffectedByLogicalCombinationsInHas() {
    has_invalidation_flags_.affected_by_logical_combinations_in_has = true;
  }
  bool AffectedByMultipleHas() const {
    return has_invalidation_flags_.affected_by_multiple_has;
  }
  void SetAffectedByMultipleHas() {
    has_invalidation_flags_.affected_by_multiple_has = true;
  }

  void Trace(blink::Visitor*) const override;

 private:
  unsigned did_attach_internals_ : 1;
  unsigned has_undo_stack_ : 1;
  unsigned scrollbar_pseudo_element_styles_depend_on_font_metrics_ : 1;
  HasInvalidationFlags has_invalidation_flags_;
  FocusgroupFlags focusgroup_flags_ = FocusgroupFlags::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
