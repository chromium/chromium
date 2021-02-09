// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/interface_data_view.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/handle_serialization.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class Message;

namespace internal {

template <typename Base, typename T>
struct Serializer<AssociatedInterfacePtrInfoDataView<Base>,
                  AssociatedInterfacePtrInfo<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(AssociatedInterfacePtrInfo<T>& input,
                        AssociatedInterface_Data* output,
                        Message* message) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    SerializeAssociatedInterfaceInfo(input.PassHandle(), input.version(),
                                     *message, *output);
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          AssociatedInterfacePtrInfo<T>* output,
                          Message* message) {
    auto handle = DeserializeAssociatedEndpointHandle(input->handle, *message);
    if (!handle.is_valid()) {
      *output = AssociatedInterfacePtrInfo<T>();
    } else {
      output->set_handle(std::move(handle));
      output->set_version(input->version);
    }
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<AssociatedInterfacePtrInfoDataView<Base>,
                  PendingAssociatedRemote<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingAssociatedRemote<T>& input,
                        AssociatedInterface_Data* output,
                        Message* message) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    SerializeAssociatedInterfaceInfo(input.PassHandle(), input.version(),
                                     *message, *output);
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          PendingAssociatedRemote<T>* output,
                          Message* message) {
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
                  AssociatedInterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(AssociatedInterfaceRequest<T>& input,
                        AssociatedEndpointHandle_Data* output,
                        Message* message) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    SerializeAssociatedEndpoint(input.PassHandle(), *message, *output);
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          AssociatedInterfaceRequest<T>* output,
                          Message* message) {
    auto handle = DeserializeAssociatedEndpointHandle(*input, *message);
    if (!handle.is_valid())
      *output = AssociatedInterfaceRequest<T>();
    else
      *output = AssociatedInterfaceRequest<T>(std::move(handle));
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
    SerializeAssociatedEndpoint(input.PassHandle(), *message, *output);
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          PendingAssociatedReceiver<T>* output,
                          Message* message) {
    auto handle = DeserializeAssociatedEndpointHandle(*input, *message);
    if (!handle.is_valid())
      *output = PendingAssociatedReceiver<T>();
    else
      *output = PendingAssociatedReceiver<T>(std::move(handle));
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, InterfacePtr<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(InterfacePtr<T>& input,
                        Interface_Data* output,
                        Message* message) {
    InterfacePtrInfo<T> info = input.PassInterface();
    SerializeInterfaceInfo(info.PassHandle(), info.version(), *message,
                           *output);
  }

  static bool Deserialize(Interface_Data* input,
                          InterfacePtr<T>* output,
                          Message* message) {
    output->Bind(InterfacePtrInfo<T>(
        DeserializeHandleAs<MessagePipeHandle>(input->handle, *message),
        input->version));
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, InterfacePtrInfo<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(InterfacePtrInfo<T>& input,
                        Interface_Data* output,
                        Message* message) {
    SerializeInterfaceInfo(input.PassHandle(), input.version(), *message,
                           *output);
  }

  static bool Deserialize(Interface_Data* input,
                          InterfacePtrInfo<T>* output,
                          Message* message) {
    *output = InterfacePtrInfo<T>(
        DeserializeHandleAs<MessagePipeHandle>(input->handle, *message),
        input->version);
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, PendingRemote<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingRemote<T>& input,
                        Interface_Data* output,
                        Message* message) {
    // |PassPipe()| invalidates all state, so capture |version()| first.
    uint32_t version = input.version();
    SerializeInterfaceInfo(input.PassPipe(), version, *message, *output);
  }

  static bool Deserialize(Interface_Data* input,
                          PendingRemote<T>* output,
                          Message* message) {
    *output = PendingRemote<T>(
        DeserializeHandleAs<MessagePipeHandle>(input->handle, *message),
        input->version);
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfaceRequestDataView<Base>, InterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(InterfaceRequest<T>& input,
                        Handle_Data* output,
                        Message* message) {
    SerializeHandle(ScopedHandle::From(input.PassMessagePipe()), *message,
                    *output);
  }

  static bool Deserialize(Handle_Data* input,
                          InterfaceRequest<T>* output,
                          Message* message) {
    DeserializeHandleAsReceiver(*input, *message, *output->internal_state());
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfaceRequestDataView<Base>, PendingReceiver<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(PendingReceiver<T>& input,
                        Handle_Data* output,
                        Message* message) {
    SerializeHandle(ScopedHandle::From(input.PassPipe()), *message, *output);
  }

  static bool Deserialize(Handle_Data* input,
                          PendingReceiver<T>* output,
                          Message* message) {
    DeserializeHandleAsReceiver(*input, *message, *output->internal_state());
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_H_
