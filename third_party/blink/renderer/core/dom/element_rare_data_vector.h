// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_base.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"

namespace blink {

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
class CORE_EXPORT ElementRareDataVector final : public ElementRareDataBase {
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
    kLastIntrinsicSize = 19,
    kPopoverData = 20,
    kToggleMap = 21,
    kPartNamesMap = 22,
    kNonce = 23,
    kIsValue = 24,
    kSavedLayerScrollOffset = 25,
    kAnchorScrollData = 26,
    kAnchoredPopoverCount = 27,

    kNumFields = 28,
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

 public:
  explicit ElementRareDataVector(NodeRenderingData*);
  ~ElementRareDataVector() override;

  void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom) override;
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const override;
  PseudoElementData::PseudoElementVector GetPseudoElements() const override;

  CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(
      Element* owner_element) override;

  ShadowRoot* GetShadowRoot() const override;
  void SetShadowRoot(ShadowRoot& shadow_root) override;

  NamedNodeMap* AttributeMap() const override;
  void SetAttributeMap(NamedNodeMap* attribute_map) override;

  DOMTokenList* GetClassList() const override;
  void SetClassList(DOMTokenList* class_list) override;

  DatasetDOMStringMap* Dataset() const override;
  void SetDataset(DatasetDOMStringMap* dataset) override;

  ScrollOffset SavedLayerScrollOffset() const override;
  void SetSavedLayerScrollOffset(ScrollOffset offset) override;

  ElementAnimations* GetElementAnimations() override;
  void SetElementAnimations(ElementAnimations* element_animations) override;

  bool HasPseudoElements() const override;
  void ClearPseudoElements() override;

  AttrNodeList& EnsureAttrNodeList() override;
  AttrNodeList* GetAttrNodeList() override;
  void RemoveAttrNodeList() override;
  void AddAttr(Attr* attr) override;

  ElementIntersectionObserverData* IntersectionObserverData() const override;
  ElementIntersectionObserverData& EnsureIntersectionObserverData() override;

  ContainerQueryEvaluator* GetContainerQueryEvaluator() const override;
  void SetContainerQueryEvaluator(ContainerQueryEvaluator* evaluator) override;

  const AtomicString& GetNonce() const override;
  void SetNonce(const AtomicString& nonce) override;

  const AtomicString& IsValue() const override;
  void SetIsValue(const AtomicString& is_value) override;

  EditContext* GetEditContext() const override;
  void SetEditContext(EditContext* edit_context) override;

  void SetPart(DOMTokenList* part) override;
  DOMTokenList* GetPart() const override;

  void SetPartNamesMap(const AtomicString part_names) override;
  const NamesMap* PartNamesMap() const override;

  InlineStylePropertyMap& EnsureInlineStylePropertyMap(
      Element* owner_element) override;
  InlineStylePropertyMap* GetInlineStylePropertyMap() override;

  const ElementInternals* GetElementInternals() const override;
  ElementInternals& EnsureElementInternals(HTMLElement& target) override;

  AccessibleNode* GetAccessibleNode() const override;
  AccessibleNode* EnsureAccessibleNode(Element* owner_element) override;
  void ClearAccessibleNode() override;

  DisplayLockContext* EnsureDisplayLockContext(Element* element) override;
  DisplayLockContext* GetDisplayLockContext() const override;

  ContainerQueryData& EnsureContainerQueryData() override;
  ContainerQueryData* GetContainerQueryData() const override;
  void ClearContainerQueryData() override;

  // Returns the crop-ID if one was set, or nullptr otherwise.
  const RegionCaptureCropId* GetRegionCaptureCropId() const override;
  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  void SetRegionCaptureCropId(
      std::unique_ptr<RegionCaptureCropId> crop_id) override;

  ResizeObserverDataMap* ResizeObserverData() const override;
  ResizeObserverDataMap& EnsureResizeObserverData() override;

  void SetCustomElementDefinition(CustomElementDefinition* definition) override;
  CustomElementDefinition* GetCustomElementDefinition() const override;

  void SaveLastIntrinsicSize(ResizeObserverSize* size) override;
  const ResizeObserverSize* LastIntrinsicSize() const override;

  PopoverData* GetPopoverData() const override;
  PopoverData& EnsurePopoverData() override;
  void RemovePopoverData() override;

  CSSToggleMap* GetToggleMap() const override;
  CSSToggleMap& EnsureToggleMap(Element* owner_element) override;

  bool HasElementFlag(ElementFlags mask) const override {
    return element_flags_ & static_cast<uint16_t>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) override {
    element_flags_ =
        (element_flags_ & ~static_cast<uint16_t>(mask)) |
        (-static_cast<uint16_t>(value) & static_cast<uint16_t>(mask));
  }
  void ClearElementFlag(ElementFlags mask) override {
    element_flags_ &= ~static_cast<uint16_t>(mask);
  }

  bool HasRestyleFlags() const override {
    return bit_field_.get<RestyleFlags>();
  }
  void ClearRestyleFlags() override { bit_field_.set<RestyleFlags>(0); }

  void SetTabIndexExplicitly() override {
    SetElementFlag(ElementFlags::kTabIndexWasSetExplicitly, true);
  }
  void ClearTabIndexExplicitly() override {
    ClearElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
  }

  AnchorScrollData* GetAnchorScrollData() const override;
  void RemoveAnchorScrollData() override;
  AnchorScrollData& EnsureAnchorScrollData(Element*) override;

  void IncrementAnchoredPopoverCount() override;
  void DecrementAnchoredPopoverCount() override;
  bool HasAnchoredPopover() const override;

  void Trace(blink::Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_VECTOR_H_
