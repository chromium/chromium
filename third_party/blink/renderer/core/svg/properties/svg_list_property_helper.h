/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_LIST_PROPERTY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_LIST_PROPERTY_HELPER_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This is an implementation of the SVG*List property spec:
// http://www.w3.org/TR/SVG/single-page.html#types-InterfaceSVGLengthList
template <typename Derived, typename ItemProperty>
class SVGListPropertyHelper : public SVGPropertyHelper<Derived> {
 public:
  typedef ItemProperty ItemPropertyType;

  SVGListPropertyHelper() = default;

  ~SVGListPropertyHelper() override = default;

  // used from Blink C++ code:

  ItemPropertyType* at(uint32_t index) {
    DCHECK_LT(index, values_.size());
    DCHECK_EQ(values_.at(index)->OwnerList(), this);
    return values_.at(index).Get();
  }

  const ItemPropertyType* at(uint32_t index) const {
    return const_cast<SVGListPropertyHelper<Derived, ItemProperty>*>(this)->at(
        index);
  }

  class ConstIterator {
    STACK_ALLOCATED();

   private:
    typedef typename HeapVector<Member<ItemPropertyType>>::const_iterator
        WrappedType;

   public:
    ConstIterator(WrappedType it) : it_(it) {}

    ConstIterator& operator++() {
      ++it_;
      return *this;
    }

    bool operator==(const ConstIterator& o) const { return it_ == o.it_; }
    bool operator!=(const ConstIterator& o) const { return it_ != o.it_; }

    ItemPropertyType* operator*() { return *it_; }
    ItemPropertyType* operator->() { return *it_; }

   private:
    WrappedType it_;
  };

  ConstIterator begin() const { return ConstIterator(values_.begin()); }
  ConstIterator end() const { return ConstIterator(values_.end()); }

  void Append(ItemPropertyType* new_item) {
    DCHECK(new_item);
    values_.push_back(new_item);
    new_item->SetOwnerList(this);
  }

  bool operator==(const Derived& other) const {
    return values_ == other.values_;
  }
  bool operator!=(const Derived& other) const { return !(*this == other); }

  bool IsEmpty() const { return !length(); }

  virtual Derived* Clone() {
    auto* svg_list = MakeGarbageCollected<Derived>();
    svg_list->DeepCopy(static_cast<Derived*>(this));
    return svg_list;
  }

  // SVGList*Property DOM spec:

  uint32_t length() const { return values_.size(); }

  void Clear();

  ItemPropertyType* Initialize(ItemPropertyType*);
  ItemPropertyType* GetItem(uint32_t, ExceptionState&);
  ItemPropertyType* InsertItemBefore(ItemPropertyType*, uint32_t);
  ItemPropertyType* RemoveItem(uint32_t, ExceptionState&);
  ItemPropertyType* AppendItem(ItemPropertyType*);
  ItemPropertyType* ReplaceItem(ItemPropertyType*, uint32_t, ExceptionState&);

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(values_);
    SVGPropertyHelper<Derived>::Trace(visitor);
  }

 protected:
  void DeepCopy(Derived*);

  bool AdjustFromToListValues(Derived* from_list,
                              Derived* to_list,
                              float percentage,
                              bool is_to_animation);

  String SerializeList() const;

  virtual ItemPropertyType* CreatePaddingItem() const {
    return MakeGarbageCollected<ItemPropertyType>();
  }

 private:
  inline bool CheckIndexBound(uint32_t, ExceptionState&);

  HeapVector<Member<ItemPropertyType>> values_;
};

template <typename Derived, typename ItemProperty>
void SVGListPropertyHelper<Derived, ItemProperty>::Clear() {
  // Detach all list items as they are no longer part of this list.
  for (auto& value : values_) {
    DCHECK_EQ(value->OwnerList(), this);
    value->SetOwnerList(nullptr);
  }
  values_.clear();
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::Initialize(
    ItemProperty* new_item) {
  // Spec: Clears all existing current items from the list and re-initializes
  // the list to hold the single item specified by the parameter.
  Clear();
  Append(new_item);
  return new_item;
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::GetItem(
    uint32_t index,
    ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return nullptr;
  return at(index);
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::InsertItemBefore(
    ItemProperty* new_item,
    uint32_t index) {
  // Spec: If the index is greater than or equal to length, then the new item is
  // appended to the end of the list.
  if (index > values_.size())
    index = values_.size();

  // Spec: Inserts a new item into the list at the specified position. The index
  // of the item before which the new item is to be inserted. The first item is
  // number 0. If the index is equal to 0, then the new item is inserted at the
  // front of the list.
  values_.insert(index, new_item);
  new_item->SetOwnerList(this);
  return new_item;
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::RemoveItem(
    uint32_t index,
    ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return nullptr;

  DCHECK_EQ(values_.at(index)->OwnerList(), this);
  ItemPropertyType* old_item = values_.at(index);
  values_.EraseAt(index);
  old_item->SetOwnerList(nullptr);
  return old_item;
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::AppendItem(
    ItemProperty* new_item) {
  // Append the value and wrapper at the end of the list.
  Append(new_item);
  return new_item;
}

template <typename Derived, typename ItemProperty>
ItemProperty* SVGListPropertyHelper<Derived, ItemProperty>::ReplaceItem(
    ItemProperty* new_item,
    uint32_t index,
    ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return nullptr;

  // Update the value at the desired position 'index'.
  Member<ItemPropertyType>& position = values_[index];
  DCHECK_EQ(position->OwnerList(), this);
  position->SetOwnerList(nullptr);
  position = new_item;
  new_item->SetOwnerList(this);
  return new_item;
}

template <typename Derived, typename ItemProperty>
bool SVGListPropertyHelper<Derived, ItemProperty>::CheckIndexBound(
    uint32_t index,
    ExceptionState& exception_state) {
  if (index >= values_.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index,
                                                    values_.size()));
    return false;
  }
  return true;
}

template <typename Derived, typename ItemProperty>
void SVGListPropertyHelper<Derived, ItemProperty>::DeepCopy(Derived* from) {
  Clear();
  for (const auto& from_value : from->values_)
    Append(from_value->Clone());
}

template <typename Derived, typename ItemProperty>
bool SVGListPropertyHelper<Derived, ItemProperty>::AdjustFromToListValues(
    Derived* from_list,
    Derived* to_list,
    float percentage,
    bool is_to_animation) {
  // If no 'to' value is given, nothing to animate.
  uint32_t to_list_size = to_list->length();
  if (!to_list_size)
    return false;

  // If the 'from' value is given and it's length doesn't match the 'to' value
  // list length, fallback to a discrete animation.
  uint32_t from_list_size = from_list->length();
  if (from_list_size != to_list_size && from_list_size) {
    if (percentage < 0.5) {
      if (!is_to_animation)
        DeepCopy(from_list);
    } else {
      DeepCopy(to_list);
    }
    return false;
  }

  DCHECK(!from_list_size || from_list_size == to_list_size);
  for (uint32_t i = length(); i < to_list_size; ++i)
    Append(CreatePaddingItem());

  return true;
}

template <typename Derived, typename ItemProperty>
String SVGListPropertyHelper<Derived, ItemProperty>::SerializeList() const {
  if (values_.IsEmpty())
    return String();

  StringBuilder builder;

  auto it = values_.begin();
  auto it_end = values_.end();
  while (it != it_end) {
    builder.Append((*it)->ValueAsString());
    ++it;
    if (it != it_end)
      builder.Append(' ');
  }
  return builder.ToString();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_LIST_PROPERTY_HELPER_H_
