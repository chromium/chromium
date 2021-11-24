// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/observable_array_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"

namespace blink {

namespace bindings {

// The implementation class of ObservableArrayExoticObject.
class CORE_EXPORT ObservableArrayExoticObjectImpl final
    : public ObservableArrayExoticObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Returns the backing list object extracted from the proxy target object
  // of type JS Array.
  static bindings::ObservableArrayBase* ProxyTargetToObservableArrayBase(
      v8::Isolate* isolate,
      v8::Local<v8::Array> v8_proxy_target);

  explicit ObservableArrayExoticObjectImpl(
      bindings::ObservableArrayBase* observable_array_backing_list_object);
  ~ObservableArrayExoticObjectImpl() override = default;

  // ScriptWrappable overrides
  v8::MaybeLocal<v8::Value> Wrap(ScriptState* script_state) override;
  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate* isolate,
      const WrapperTypeInfo* wrapper_type_info,
      v8::Local<v8::Object> wrapper) override;

 private:
  static const WrapperTypeInfo wrapper_type_info_body_;
  static const V8PrivateProperty::SymbolKey kV8ProxyTargetToV8WrapperKey;
};

template <typename ElementType>
class ObservableArrayImplHelper : public bindings::ObservableArrayBase {
 public:
  using BackingListType = VectorOf<ElementType>;
  using size_type = uint32_t;
  using value_type = ElementType;
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
  wtf_size_t size() const { return backing_list_.size(); }
  wtf_size_t capacity() const { return backing_list_.capacity(); }
  bool IsEmpty() const { return backing_list_.IsEmpty(); }
  void ReserveCapacity(size_type new_capacity) {
    backing_list_.ReserveCapacity(new_capacity);
  }
  void ReserveInitialCapacity(size_type initial_capacity) {
    backing_list_.ReserveInitialCapacity(initial_capacity);
  }
  value_type& at(size_type index) { return backing_list_.at(index); }
  const value_type& at(size_type index) const {
    return backing_list_.at(index);
  }
  value_type& operator[](size_type index) { return backing_list_[index]; }
  const value_type& operator[](size_type index) const {
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
  value_type& front() { return backing_list_.front(); }
  value_type& back() { return backing_list_.back(); }
  const value_type& front() const { return backing_list_.front(); }
  const value_type& back() const { return backing_list_.back(); }
  // Vector-compatible APIs (modifiers)
  void resize(size_type size) { backing_list_.resize(size); }
  void clear() { backing_list_.clear(); }
  template <typename T>
  void push_back(T&& value) {
    backing_list_.push_back(std::forward<T>(value));
  }
  void pop_back() { backing_list_.pop_back(); }
  template <typename... Args>
  value_type& emplace_back(Args&&... args) {
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
