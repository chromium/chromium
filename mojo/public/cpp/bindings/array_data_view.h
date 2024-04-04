// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_

#include <type_traits>

#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"

namespace mojo {

class Message;

namespace internal {

template <typename T, typename EnableType = void>
class ArrayDataViewImpl;

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kPOD>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  T operator[](size_t index) const { return data_->at(index); }

  const T* data() const { return data_->storage(); }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kBoolean>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  bool operator[](size_t index) const { return data_->at(index); }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
  requires(!base::is_instantiation<std::optional, T>)
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kEnum>::value>::type> {
 public:
  static_assert(std::is_same<std::underlying_type_t<T>, int32_t>::value,
                "Unexpected enum type");

  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  T operator[](size_t index) const {
    return ToKnownEnumValueHelper(static_cast<T>(data_->at(index)));
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<T>(data_->at(index), output);
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
  requires(base::is_instantiation<std::optional, T>)
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kEnum>::value>::type> {
 public:
  static_assert(std::is_same<std::underlying_type_t<typename T::value_type>,
                             int32_t>::value,
                "Unexpected enum type");

  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  T operator[](size_t index) const {
    auto value = static_cast<std::optional<int32_t>>(data_->at(index));
    if (!value) {
      return std::nullopt;
    } else {
      return ToKnownEnumValueHelper(
          static_cast<typename T::value_type>(*value));
    }
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<T>(data_->at(index), output);
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T,
                  MojomTypeCategory::kAssociatedInterface |
                      MojomTypeCategory::kAssociatedInterfaceRequest |
                      MojomTypeCategory::kInterface |
                      MojomTypeCategory::kInterfaceRequest>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  template <typename U>
  U Take(size_t index) {
    U result;
    bool ret = Deserialize<T>(&data_->at(index), &result, message_);
    DCHECK(ret);
    return result;
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kHandle>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  T Take(size_t index) {
    T result;
    bool ret = Deserialize<T>(&data_->at(index), &result, message_);
    DCHECK(ret);
    return result;
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T,
                  MojomTypeCategory::kArray | MojomTypeCategory::kMap |
                      MojomTypeCategory::kString |
                      MojomTypeCategory::kStruct>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  void GetDataView(size_t index, T* output) {
    *output = T(data_->at(index).Get(), message_);
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<T>(data_->at(index).Get(), output, message_);
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<T, MojomTypeCategory::kUnion>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<ArrayDataView<T>>::Data;

  ArrayDataViewImpl(Data_* data, Message* message)
      : data_(data), message_(message) {}

  void GetDataView(size_t index, T* output) {
    *output = T(&data_->at(index), message_);
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<T>(&data_->at(index), output, message_);
  }

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Data_* data_;
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data.
  RAW_PTR_EXCLUSION Message* message_;
};

}  // namespace internal

template <typename K, typename V>
class MapDataView;

template <typename T>
class ArrayDataView : public internal::ArrayDataViewImpl<T> {
 public:
  using Element = T;
  using Data_ = typename internal::ArrayDataViewImpl<T>::Data_;

  ArrayDataView() : internal::ArrayDataViewImpl<T>(nullptr, nullptr) {}

  ArrayDataView(Data_* data, Message* message)
      : internal::ArrayDataViewImpl<T>(data, message) {}

  bool is_null() const { return !this->data_; }

  size_t size() const { return this->data_->size(); }

  // Methods to access elements are different for different element types. They
  // are inherited from internal::ArrayDataViewImpl:

  // POD types except boolean and enums:
  //   T operator[](size_t index) const;
  //   const T* data() const;

  // Boolean:
  //   bool operator[](size_t index) const;

  // Enums:
  //   T operator[](size_t index) const;
  //   template <typename U>
  //   bool Read(size_t index, U* output);

  // Handles:
  //   T Take(size_t index);

  // Interfaces:
  //   template <typename U>
  //   U Take(size_t index);

  // Object types:
  //   void GetDataView(size_t index, T* output);
  //   template <typename U>
  //   bool Read(size_t index, U* output);

 private:
  template <typename K, typename V>
  friend class MapDataView;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_
