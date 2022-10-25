// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/native_handle_type_converters.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"

namespace mojo {
namespace internal {

// static
void UnmappedNativeStructSerializerImpl::Serialize(
    const native::NativeStructPtr& input,
    MessageFragment<native::internal::NativeStruct_Data>& fragment) {
  if (!input)
    return;

  fragment.Allocate();
  MessageFragment<Array_Data<uint8_t>> data_fragment(fragment.message());
  constexpr const ContainerValidateParams& data_validate_params =
      GetArrayValidator<0, false, nullptr>();
  mojo::internal::Serialize<ArrayDataView<uint8_t>>(input->data, data_fragment,
                                                    &data_validate_params);
  fragment->data.Set(data_fragment.data());

  MessageFragment<Array_Data<Pointer<native::internal::SerializedHandle_Data>>>
      handles_fragment(fragment.message());
  constexpr const ContainerValidateParams& handles_validate_params =
      GetArrayValidator<0, false, nullptr>();
  mojo::internal::Serialize<
      mojo::ArrayDataView<::mojo::native::SerializedHandleDataView>>(
      input->handles, handles_fragment, &handles_validate_params);
  fragment->handles.Set(handles_fragment.is_null() ? nullptr
                                                   : handles_fragment.data());
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
    MessageFragment<native::internal::NativeStruct_Data>& fragment) {
  fragment.Allocate();

  // Allocate a uint8 array, initialize its header, and copy the Pickle in.
  MessageFragment<Array_Data<uint8_t>> data_fragment(fragment.message());
  data_fragment.AllocateArrayData(ipc_message->payload_size());
  memcpy(data_fragment->storage(), ipc_message->payload(),
         ipc_message->payload_size());
  fragment->data.Set(data_fragment.data());

  if (ipc_message->attachment_set()->empty()) {
    fragment->handles.Set(nullptr);
    return;
  }

  MessageFragment<Array_Data<Pointer<native::internal::SerializedHandle_Data>>>
      handles_fragment(fragment.message());
  auto* attachments = ipc_message->attachment_set();
  handles_fragment.AllocateArrayData(attachments->size());
  for (unsigned i = 0; i < attachments->size(); ++i) {
    MessageFragment<native::internal::SerializedHandle_Data> handle_fragment(
        fragment.message());
    handle_fragment.Allocate();

    auto attachment = attachments->GetAttachmentAt(i);
    ScopedHandle handle = attachment->TakeMojoHandle();
    internal::Serializer<ScopedHandle, ScopedHandle>::Serialize(
        handle, &handle_fragment->the_handle, &fragment.message());
    handle_fragment->type = static_cast<int32_t>(
        mojo::ConvertTo<native::SerializedHandleType>(attachment->GetType()));
    handles_fragment->at(i).Set(handle_fragment.data());
  }
  fragment->handles.Set(handles_fragment.data());
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
