// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_SERIALIZATION_H_

#include "mojo/public/cpp/bindings/lib/array_serialization_send_validation.h"
#include "mojo/public/cpp/bindings/lib/handle_serialization_send_validation.h"
#include "mojo/public/cpp/bindings/lib/map_serialization_send_validation.h"
#include "mojo/public/cpp/bindings/lib/send_validation_type.h"
#include "mojo/public/cpp/bindings/lib/string_serialization_send_validation.h"

namespace mojo::internal {

template <typename MojomType,
          SendValidation send_validation = SendValidation::kDefault,
          typename EnableType = void>
struct MojomSendValidationSerializationImplTraits;

template <typename MojomType, SendValidation send_validation>
struct MojomSendValidationSerializationImplTraits<
    MojomType,
    send_validation,
    typename std::enable_if<
        BelongsTo<MojomType, MojomTypeCategory::kStruct>::value>::type> {
  template <typename MaybeConstUserType, typename FragmentType>
  static void Serialize(MaybeConstUserType& input, FragmentType& fragment) {
    mojo::internal::Serialize<MojomType, send_validation>(input, fragment);
  }
};

template <typename MojomType, SendValidation send_validation>
struct MojomSendValidationSerializationImplTraits<
    MojomType,
    send_validation,
    typename std::enable_if<
        BelongsTo<MojomType, MojomTypeCategory::kUnion>::value>::type> {
  template <typename MaybeConstUserType, typename FragmentType>
  static void Serialize(MaybeConstUserType& input, FragmentType& fragment) {
    mojo::internal::Serialize<MojomType, send_validation>(input, fragment,
                                                          false /* inline */);
  }
};

template <typename MojomType, SendValidation send_validation, typename UserType>
mojo::Message SerializeAsMessageImpl(UserType* input) {
  // Note that this is only called by application code serializing a structure
  // manually (e.g. for storage). As such we don't want Mojo's soft message size
  // limits to be applied.
  mojo::Message message(0, 0, 0, 0, MOJO_CREATE_MESSAGE_FLAG_UNLIMITED_SIZE,
                        nullptr);
  MessageFragment<typename MojomTypeTraits<MojomType>::Data> fragment(message);
  MojomSendValidationSerializationImplTraits<
      MojomType, send_validation>::Serialize(*input, fragment);
  message.SerializeHandles(/*group_controller=*/nullptr);
  return message;
}

template <typename MojomType,
          typename DataArrayType,
          SendValidation send_validation,
          typename UserType>
DataArrayType SerializeImpl(UserType* input) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::kStruct>::value ||
                    BelongsTo<MojomType, MojomTypeCategory::kUnion>::value,
                "Unexpected type.");
  Message message = SerializeAsMessageImpl<MojomType, send_validation>(input);
  uint32_t size = message.payload_num_bytes();
  DataArrayType result(size);
  if (size) {
    memcpy(&result.front(), message.payload(), size);
  }
  return result;
}

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_SERIALIZATION_H_
