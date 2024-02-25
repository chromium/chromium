// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_H_

#include "third_party/blink/renderer/bindings/core/v8/observable_array_exotic_object_impl.h"
#include "third_party/blink/renderer/platform/bindings/observable_array_base.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"

namespace blink {

namespace bindings {

template <typename ElementType>
class ObservableArrayImplHelper : public bindings::ObservableArrayBase {
 public:
  using BackingListType = VectorOf<ElementType>;
  using size_type = typename BackingListType::size_type;
  using value_type = typename BackingListType::value_type;
  using reference = typename BackingListType::reference;
  using const_reference = typename BackingListType::const_reference;
  using pointer = typename BackingListType::pointer;
  using const_pointer = typename BackingListType::const_pointer;
  using iterator = typename BackingListType::iterator;
  using const_iterator = typename BackingListType::const_iterator;
  using reverse_iterator = typename BackingListType::reverse_iterator;
  using const_reverse_iterator =
      typename BackingListType::const_reverse_iterator;

  explicit ObservableArrayImplHelper(ScriptWrappable* platform_object)
      : bindings::ObservableArrayBase(
            platform_object,
            MakeGarbageCollected<bindings::ObservableArrayExoticObjectImpl>(
                this)) {}
  ~ObservableArrayImplHelper() override = default;

  // Returns the observable array exotic object, which is the value to be
  // returned as the IDL attribute value.  Do not use `this` object (= the
  // observable array backing list object) as the IDL attribute value.
  using bindings::ObservableArrayBase::GetExoticObject;

  // Vector-compatible APIs (accessors)
  size_type size() const { return backing_list_.size(); }
  size_type capacity() const { return backing_list_.capacity(); }
  bool empty() const { return backing_list_.empty(); }
  void reserve(size_type new_capacity) { backing_list_.reserve(new_capacity); }
  void ReserveInitialCapacity(size_type initial_capacity) {
    backing_list_.ReserveInitialCapacity(initial_capacity);
  }
  reference at(size_type index) { return backing_list_.at(index); }
  const_reference at(size_type index) const { return backing_list_.at(index); }
  reference operator[](size_type index) { return backing_list_[index]; }
  const_reference operator[](size_type index) const {
    return backing_list_[index];
  }
  value_type* data() { return backing_list_.data(); }
  const value_type* data() const { return backing_list_.data(); }
  iterator begin() { return backing_list_.begin(); }
  iterator end() { return backing_list_.end(); }
  const_iterator begin() const { return backing_list_.begin(); }
  const_iterator end() const { return backing_list_.end(); }
  reverse_iterator rbegin() { return backing_list_.rbegin(); }
  reverse_iterator rend() { return backing_list_.rend(); }
  const_reverse_iterator rbegin() const { return backing_list_.rbegin(); }
  const_reverse_iterator rend() const { return backing_list_.rend(); }
  reference front() { return backing_list_.front(); }
  reference back() { return backing_list_.back(); }
  const_reference front() const { return backing_list_.front(); }
  const_reference back() const { return backing_list_.back(); }
  // Vector-compatible APIs (modifiers)
  void resize(size_type size) { backing_list_.resize(size); }
  void clear() { backing_list_.clear(); }
  template <typename T>
  void push_back(T&& value) {
    backing_list_.push_back(std::forward<T>(value));
  }
  void pop_back() { backing_list_.pop_back(); }
  template <typename... Args>
  reference emplace_back(Args&&... args) {
    return backing_list_.emplace_back(std::forward<Args>(args)...);
  }
  template <typename T>
  void insert(iterator position, T&& value) {
    backing_list_.InsertAt(position, std::forward<T>(value));
  }
  iterator erase(iterator position) { return backing_list_.erase(position); }
  iterator erase(iterator first, iterator last) {
    return backing_list_.erase(first, last);
  }

  void Trace(Visitor* visitor) const override {
    ObservableArrayBase::Trace(visitor);
    TraceIfNeeded<BackingListType>::Trace(visitor, backing_list_);
  }

 private:
  BackingListType backing_list_;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_H_
