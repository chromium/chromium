// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/native_handle_type_converters.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"

namespace mojo {
namespace internal {

// static
void UnmappedNativeStructSerializerImpl::Serialize(
    const native::NativeStructPtr& input,
    native::internal::NativeStruct_Data::BufferWriter* writer,
    Message* message) {
  if (!input)
    return;

  writer->Allocate(message->payload_buffer());

  Array_Data<uint8_t>::BufferWriter data_writer;
  const mojo::internal::ContainerValidateParams data_validate_params(0, false,
                                                                     nullptr);
  mojo::internal::Serialize<ArrayDataView<uint8_t>>(
      input->data, &data_writer, &data_validate_params, message);
  writer->data()->data.Set(data_writer.data());

  mojo::internal::Array_Data<mojo::internal::Pointer<
      native::internal::SerializedHandle_Data>>::BufferWriter handles_writer;
  const mojo::internal::ContainerValidateParams handles_validate_params(
      0, false, nullptr);
  mojo::internal::Serialize<
      mojo::ArrayDataView<::mojo::native::SerializedHandleDataView>>(
      input->handles, &handles_writer, &handles_validate_params, message);
  writer->data()->handles.Set(handles_writer.is_null() ? nullptr
                                                       : handles_writer.data());
}

// static
bool UnmappedNativeStructSerializerImpl::Deserialize(
    native::internal::NativeStruct_Data* input,
    native::NativeStructPtr* output,
    Message* message) {
  if (!input) {
    output->reset();
    return true;
  }

  native::NativeStructDataView data_view(input, message);
  return StructTraits<::mojo::native::NativeStructDataView,
                      native::NativeStructPtr>::Read(data_view, output);
}

// static
void UnmappedNativeStructSerializerImpl::SerializeMessageContents(
    IPC::Message* ipc_message,
    native::internal::NativeStruct_Data::BufferWriter* writer,
    Message* message) {
  writer->Allocate(message->payload_buffer());

  // Allocate a uint8 array, initialize its header, and copy the Pickle in.
  Array_Data<uint8_t>::BufferWriter data_writer;
  data_writer.Allocate(ipc_message->payload_size(), message->payload_buffer());
  memcpy(data_writer->storage(), ipc_message->payload(),
         ipc_message->payload_size());
  writer->data()->data.Set(data_writer.data());

  if (ipc_message->attachment_set()->empty()) {
    writer->data()->handles.Set(nullptr);
    return;
  }

  mojo::internal::Array_Data<mojo::internal::Pointer<
      native::internal::SerializedHandle_Data>>::BufferWriter handles_writer;
  auto* attachments = ipc_message->attachment_set();
  handles_writer.Allocate(attachments->size(), message->payload_buffer());
  for (unsigned i = 0; i < attachments->size(); ++i) {
    native::internal::SerializedHandle_Data::BufferWriter handle_writer;
    handle_writer.Allocate(message->payload_buffer());

    auto attachment = attachments->GetAttachmentAt(i);
    ScopedHandle handle = attachment->TakeMojoHandle();
    internal::Serializer<ScopedHandle, ScopedHandle>::Serialize(
        handle, &handle_writer->the_handle, message);
    handle_writer->type = static_cast<int32_t>(
        mojo::ConvertTo<native::SerializedHandleType>(attachment->GetType()));
    handles_writer.data()->at(i).Set(handle_writer.data());
  }
  writer->data()->handles.Set(handles_writer.data());
}

// static
bool UnmappedNativeStructSerializerImpl::DeserializeMessageAttachments(
    native::internal::NativeStruct_Data* data,
    Message* message,
    IPC::Message* ipc_message) {
  if (data->handles.is_null())
    return true;

  auto* handles_data = data->handles.Get();
  for (size_t i = 0; i < handles_data->size(); ++i) {
    auto* handle_data = handles_data->at(i).Get();
    if (!handle_data)
      return false;
    ScopedHandle handle;
    internal::Serializer<ScopedHandle, ScopedHandle>::Deserialize(
        &handle_data->the_handle, &handle, message);
    auto attachment = IPC::MessageAttachment::CreateFromMojoHandle(
        std::move(handle),
        mojo::ConvertTo<IPC::MessageAttachment::Type>(
            static_cast<native::SerializedHandleType>(handle_data->type)));
    ipc_message->attachment_set()->AddAttachment(std::move(attachment));
  }
  return true;
}

}  // namespace internal
}  // namespace mojo
