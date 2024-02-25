// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_data_view.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/handle_serialization.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class Message;

namespace internal {

template <typename Base, typename T>
struct Serializer<AssociatedInterfacePtrInfoDataView<Base>,
                  PendingAssociatedRemote<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingAssociatedRemote<T>& input,
                        AssociatedInterface_Data* output,
                        Message* message) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      input.reset();
    }
    SerializeAssociatedInterfaceInfo(input.PassHandle(), input.version(),
                                     *message, *output);
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          PendingAssociatedRemote<T>* output,
                          Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      return false;
    }
    auto handle = DeserializeAssociatedEndpointHandle(input->handle, *message);
    if (!handle.is_valid()) {
      *output = PendingAssociatedRemote<T>();
    } else {
      output->set_handle(std::move(handle));
      output->set_version(input->version);
    }
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<AssociatedInterfaceRequestDataView<Base>,
                  PendingAssociatedReceiver<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingAssociatedReceiver<T>& input,
                        AssociatedEndpointHandle_Data* output,
                        Message* message) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      input.reset();
    }
    SerializeAssociatedEndpoint(input.PassHandle(), *message, *output);
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          PendingAssociatedReceiver<T>* output,
                          Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      return false;
    }
    auto handle = DeserializeAssociatedEndpointHandle(*input, *message);
    if (!handle.is_valid())
      *output = PendingAssociatedReceiver<T>();
    else
      *output = PendingAssociatedReceiver<T>(std::move(handle));
    return true;
  }
};

template <typename T>
struct Serializer<AssociatedInterfaceRequestDataView<T>,
                  ScopedInterfaceEndpointHandle> {
  static void Serialize(ScopedInterfaceEndpointHandle& input,
                        AssociatedEndpointHandle_Data* output,
                        Message* message) {
    DCHECK(!input.is_valid() || input.pending_association());
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      input.reset();
    }
    SerializeAssociatedEndpoint(std::move(input), *message, *output);
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          ScopedInterfaceEndpointHandle* output,
                          Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      return false;
    }
    *output = DeserializeAssociatedEndpointHandle(*input, *message);
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, PendingRemote<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingRemote<T>& input,
                        Interface_Data* output,
                        Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      input.reset();
    }
    // |PassPipe()| invalidates all state, so capture |version()| first.
    uint32_t version = input.version();
    SerializeInterfaceInfo(input.PassPipe(), version, *message, *output);
  }

  static bool Deserialize(Interface_Data* input,
                          PendingRemote<T>* output,
                          Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      return false;
    }
    *output = PendingRemote<T>(
        DeserializeHandleAs<MessagePipeHandle>(input->handle, *message),
        input->version);
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfaceRequestDataView<Base>, PendingReceiver<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingReceiver<T>& input,
                        Handle_Data* output,
                        Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      input.reset();
    }
    SerializeHandle(ScopedHandle::From(input.PassPipe()), *message, *output);
  }

  static bool Deserialize(Handle_Data* input,
                          PendingReceiver<T>* output,
                          Message* message) {
    if (!GetRuntimeFeature_ExpectEnabled<T>()) {
      return false;
    }
    DeserializeHandleAsReceiver(*input, *message, *output->internal_state());
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_
