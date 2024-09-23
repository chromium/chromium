// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "base/check_op.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/native_enum.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType>
struct NativeEnumSerializerImpl {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = IPC::ParamTraits<UserType>;

  // IPC_ENUM_TRAITS* macros serialize enum as int, make sure that fits into
  // mojo native-only enum.
  static_assert(sizeof(NativeEnum) >= sizeof(int),
                "Cannot store the serialization result in NativeEnum.");

  static void Serialize(UserType input, int32_t* output) {
    base::Pickle pickle;
    Traits::Write(&pickle, input);

    CHECK_GE(sizeof(int32_t), pickle.payload_size());
    *output = 0;
    memcpy(reinterpret_cast<char*>(output), pickle.payload_bytes().data(),
           pickle.payload_bytes().size());
  }

  struct PickleData {
    uint32_t payload_size;
    int32_t value;
  };
  static_assert(sizeof(PickleData) == 8, "PickleData size mismatch.");

  static bool Deserialize(int32_t input, UserType* output) {
    PickleData data = {sizeof(int32_t), input};
    base::Pickle pickle_view =
        base::Pickle::WithUnownedBuffer(base::byte_span_from_ref(data));
    base::PickleIterator iter(pickle_view);
    return Traits::Read(&pickle_view, &iter, output);
  }
};

struct UnmappedNativeEnumSerializerImpl {
  static void Serialize(NativeEnum input, int32_t* output) {
    *output = static_cast<int32_t>(input);
  }
  static bool Deserialize(int32_t input, NativeEnum* output) {
    *output = static_cast<NativeEnum>(input);
    return true;
  }
};

template <>
struct NativeEnumSerializerImpl<NativeEnum>
    : public UnmappedNativeEnumSerializerImpl {};

template <>
struct NativeEnumSerializerImpl<const NativeEnum>
    : public UnmappedNativeEnumSerializerImpl {};

template <typename MaybeConstUserType>
struct Serializer<NativeEnum, MaybeConstUserType>
    : public NativeEnumSerializerImpl<MaybeConstUserType> {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_SERIALIZATION_H_
