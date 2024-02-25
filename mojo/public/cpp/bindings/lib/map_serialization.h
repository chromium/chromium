// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_

#include <type_traits>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/map_data_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/map_data_view.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType>
class MapReaderBase {
 public:
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = MapTraits<UserType>;
  using MaybeConstIterator =
      decltype(Traits::GetBegin(std::declval<MaybeConstUserType&>()));

  explicit MapReaderBase(MaybeConstUserType& input)
      : input_(input), iter_(Traits::GetBegin(input_)) {}
  ~MapReaderBase() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  // Return null because key or value elements are not stored continuously in
  // memory.
  void* GetDataIfExists() { return nullptr; }

 protected:
  // RAW_PTR_EXCLUSION: Binary size increase.
  RAW_PTR_EXCLUSION MaybeConstUserType& input_;
  MaybeConstIterator iter_;
};

// Used as the UserTypeReader template parameter of ArraySerializer.
template <typename MaybeConstUserType>
class MapKeyReader : public MapReaderBase<MaybeConstUserType> {
 public:
  using Base = MapReaderBase<MaybeConstUserType>;
  using Traits = typename Base::Traits;
  using MaybeConstIterator = typename Base::MaybeConstIterator;

  explicit MapKeyReader(MaybeConstUserType& input) : Base(input) {}
  ~MapKeyReader() {}

  using GetNextResult =
      decltype(Traits::GetKey(std::declval<MaybeConstIterator&>()));
  GetNextResult GetNext() {
    GetNextResult key = Traits::GetKey(this->iter_);
    Traits::AdvanceIterator(this->iter_);
    return key;
  }
};

// Used as the UserTypeReader template parameter of ArraySerializer.
template <typename MaybeConstUserType>
class MapValueReader : public MapReaderBase<MaybeConstUserType> {
 public:
  using Base = MapReaderBase<MaybeConstUserType>;
  using Traits = typename Base::Traits;
  using MaybeConstIterator = typename Base::MaybeConstIterator;

  explicit MapValueReader(MaybeConstUserType& input) : Base(input) {}
  ~MapValueReader() {}

  using GetNextResult =
      decltype(Traits::GetValue(std::declval<MaybeConstIterator&>()));
  GetNextResult GetNext() {
    GetNextResult value = Traits::GetValue(this->iter_);
    Traits::AdvanceIterator(this->iter_);
    return value;
  }
};

template <typename Key, typename Value, typename MaybeConstUserType>
struct Serializer<MapDataView<Key, Value>, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = MapTraits<UserType>;
  using UserKey = typename Traits::Key;
  using UserValue = typename Traits::Value;
  using Data = typename MojomTypeTraits<MapDataView<Key, Value>>::Data;
  using KeyArraySerializer = ArraySerializer<ArrayDataView<Key>,
                                             std::vector<UserKey>,
                                             MapKeyReader<MaybeConstUserType>>;
  using ValueArraySerializer =
      ArraySerializer<ArrayDataView<Value>,
                      std::vector<UserValue>,
                      MapValueReader<MaybeConstUserType>>;

  static void Serialize(MaybeConstUserType& input,
                        MessageFragment<Data>& fragment,
                        const ContainerValidateParams* validate_params) {
    DCHECK(validate_params->key_validate_params);
    DCHECK(validate_params->element_validate_params);
    if (CallIsNullIfExists<Traits>(input))
      return;

    fragment.Allocate();
    MessageFragment<typename MojomTypeTraits<ArrayDataView<Key>>::Data>
        keys_fragment(fragment.message());
    keys_fragment.AllocateArrayData(Traits::GetSize(input));
    MapKeyReader<MaybeConstUserType> key_reader(input);
    KeyArraySerializer::SerializeElements(&key_reader, keys_fragment,
                                          validate_params->key_validate_params);
    fragment->keys.Set(keys_fragment.data());

    MessageFragment<typename MojomTypeTraits<ArrayDataView<Value>>::Data>
        values_fragment(fragment.message());
    values_fragment.AllocateArrayData(Traits::GetSize(input));
    MapValueReader<MaybeConstUserType> value_reader(input);
    ValueArraySerializer::SerializeElements(
        &value_reader, values_fragment,
        validate_params->element_validate_params);
    fragment->values.Set(values_fragment.data());
  }

  static bool Deserialize(Data* input, UserType* output, Message* message) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);

    std::vector<UserKey> keys;
    std::vector<UserValue> values;

    if (!KeyArraySerializer::DeserializeElements(input->keys.Get(), &keys,
                                                 message) ||
        !ValueArraySerializer::DeserializeElements(input->values.Get(), &values,
                                                   message)) {
      return false;
    }

    DCHECK_EQ(keys.size(), values.size());
    size_t size = keys.size();
    Traits::SetToEmpty(output);

    for (size_t i = 0; i < size; ++i) {
      if (!Traits::Insert(*output, std::move(keys[i]), std::move(values[i])))
        return false;
    }
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_
