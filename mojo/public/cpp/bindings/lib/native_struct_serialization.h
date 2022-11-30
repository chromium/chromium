// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/pickle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace mojo {
namespace internal {

// Base class for the templated native struct serialization interface below,
// used to consolidated some shared logic and provide a basic
// Serialize/Deserialize for [Native] mojom structs which do not have a
// registered typemap in the current configuration (i.e. structs that are
// represented by a raw native::NativeStruct mojom struct in C++ bindings.)
struct COMPONENT_EXPORT(MOJO_CPP_BINDINGS) UnmappedNativeStructSerializerImpl {
  static void Serialize(
      const native::NativeStructPtr& input,
      MessageFragment<native::internal::NativeStruct_Data>& fragment);

  static bool Deserialize(native::internal::NativeStruct_Data* input,
                          native::NativeStructPtr* output,
                          Message* message);

  static void SerializeMessageContents(
      IPC::Message* ipc_message,
      MessageFragment<native::internal::NativeStruct_Data>& fragment);

  static bool DeserializeMessageAttachments(
      native::internal::NativeStruct_Data* data,
      Message* message,
      IPC::Message* ipc_message);
};

template <typename MaybeConstUserType>
struct NativeStructSerializerImpl {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = IPC::ParamTraits<UserType>;

  static void Serialize(
      MaybeConstUserType& value,
      MessageFragment<native::internal::NativeStruct_Data>& fragment) {
    IPC::Message ipc_message;
    Traits::Write(&ipc_message, value);
    UnmappedNativeStructSerializerImpl::SerializeMessageContents(&ipc_message,
                                                                 fragment);
  }

  static bool Deserialize(native::internal::NativeStruct_Data* data,
                          UserType* out,
                          Message* message) {
    if (!data)
      return false;

    // Construct a temporary base::Pickle view over the array data. Note that
    // the Array_Data is laid out like this:
    //
    //   [num_bytes (4 bytes)] [num_elements (4 bytes)] [elements...]
    //
    // and base::Pickle expects to view data like this:
    //
    //   [payload_size (4 bytes)] [header bytes ...] [payload...]
    //
    // Because ArrayHeader's num_bytes includes the length of the header and
    // Pickle's payload_size does not, we need to adjust the stored value
    // momentarily so Pickle can view the data.
    ArrayHeader* header = reinterpret_cast<ArrayHeader*>(data->data.Get());
    DCHECK_GE(header->num_bytes, sizeof(ArrayHeader));
    header->num_bytes -= sizeof(ArrayHeader);

    {
      // Construct a view over the full Array_Data, including our hacked up
      // header. Pickle will infer from this that the header is 8 bytes long,
      // and the payload will contain all of the pickled bytes.
      IPC::Message message_view(reinterpret_cast<const char*>(header),
                                header->num_bytes + sizeof(ArrayHeader));
      base::PickleIterator iter(message_view);
      if (!UnmappedNativeStructSerializerImpl::DeserializeMessageAttachments(
              data, message, &message_view)) {
        return false;
      }

      if (!Traits::Read(&message_view, &iter, out))
        return false;
    }

    // Return the header to its original state.
    header->num_bytes += sizeof(ArrayHeader);

    return true;
  }
};

template <>
struct NativeStructSerializerImpl<native::NativeStructPtr>
    : public UnmappedNativeStructSerializerImpl {};

template <>
struct NativeStructSerializerImpl<const native::NativeStructPtr>
    : public UnmappedNativeStructSerializerImpl {};

template <typename MaybeConstUserType>
struct Serializer<native::NativeStructDataView, MaybeConstUserType>
    : public NativeStructSerializerImpl<MaybeConstUserType> {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
