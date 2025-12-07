// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_SEND_VALIDATION_H_

#include <type_traits>
#include <vector>

#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/array_serialization_send_validation.h"
#include "mojo/public/cpp/bindings/lib/map_data_internal.h"
#include "mojo/public/cpp/bindings/lib/map_serialization.h"
#include "mojo/public/cpp/bindings/lib/send_validation.h"
#include "mojo/public/cpp/bindings/lib/send_validation_type.h"
#include "mojo/public/cpp/bindings/map_data_view.h"

namespace mojo::internal {

template <typename Key,
          typename Value,
          typename MaybeConstUserType,
          SendValidation send_validation>
struct SendValidationSerializer<MapDataView<Key, Value>,
                                MaybeConstUserType,
                                send_validation> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = MapTraits<UserType>;
  using UserKey = typename Traits::Key;
  using UserValue = typename Traits::Value;
  using Data = typename MojomTypeTraits<MapDataView<Key, Value>>::Data;
  using KeyArraySerializer =
      SelectArraySerializer<ArrayDataView<Key>,
                            std::vector<UserKey>,
                            send_validation,
                            MapKeyReader<MaybeConstUserType>>;
  using ValueArraySerializer =
      SelectArraySerializer<ArrayDataView<Value>,
                            std::vector<UserValue>,
                            send_validation,
                            MapValueReader<MaybeConstUserType>>;

  static void Serialize(MaybeConstUserType& input,
                        MessageFragment<Data>& fragment,
                        const ContainerValidateParams* validate_params) {
    DCHECK(validate_params->key_validate_params);
    DCHECK(validate_params->element_validate_params);
    if (CallIsNullIfExists<Traits>(input)) {
      return;
    }

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
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_SEND_VALIDATION_H_
