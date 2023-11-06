// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/mojo/mojo.h"

#include <memory>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/accessibility/features/mojo/mojo_handle.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "services/accessibility/features/v8_manager.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace ax {

// static
gin::WrapperInfo Mojo::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
gin::Handle<Mojo> Mojo::Create(v8::Local<v8::Context> context) {
  return gin::CreateHandle(context->GetIsolate(), new Mojo(context));
}

gin::ObjectTemplateBuilder Mojo::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<Mojo>::GetObjectTemplateBuilder(isolate)
      .SetMethod("bindInterface", &Mojo::BindInterface)
      .SetMethod("createMessagePipe", &Mojo::CreateMessagePipe)
      .SetValue("RESULT_OK", MOJO_RESULT_OK)
      .SetValue("RESULT_CANCELLED", MOJO_RESULT_CANCELLED)
      .SetValue("RESULT_UNKNOWN", MOJO_RESULT_UNKNOWN)
      .SetValue("RESULT_INVALID_ARGUMENT", MOJO_RESULT_INVALID_ARGUMENT)
      .SetValue("RESULT_DEADLINE_EXCEEDED", MOJO_RESULT_DEADLINE_EXCEEDED)
      .SetValue("RESULT_NOT_FOUND", MOJO_RESULT_NOT_FOUND)
      .SetValue("RESULT_ALREADY_EXISTS", MOJO_RESULT_ALREADY_EXISTS)
      .SetValue("RESULT_PERMISSION_DENIED", MOJO_RESULT_PERMISSION_DENIED)
      .SetValue("RESULT_RESOURCE_EXHAUSTED", MOJO_RESULT_RESOURCE_EXHAUSTED)
      .SetValue("RESULT_FAILED_PRECONDITION", MOJO_RESULT_FAILED_PRECONDITION)
      .SetValue("RESULT_ABORTED", MOJO_RESULT_ABORTED)
      .SetValue("RESULT_OUT_OF_RANGE", MOJO_RESULT_OUT_OF_RANGE)
      .SetValue("RESULT_UNIMPLEMENTED", MOJO_RESULT_UNIMPLEMENTED)
      .SetValue("RESULT_INTERNAL", MOJO_RESULT_INTERNAL)
      .SetValue("RESULT_UNAVAILABLE", MOJO_RESULT_UNAVAILABLE)
      .SetValue("RESULT_DATA_LOSS", MOJO_RESULT_DATA_LOSS)
      .SetValue("RESULT_BUSY", MOJO_RESULT_BUSY)
      .SetValue("RESULT_SHOULD_WAIT", MOJO_RESULT_SHOULD_WAIT);
}

void Mojo::CreateMessagePipe(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  CHECK(arguments);

  MojoCreateMessagePipeOptions options = {/*struct_size=*/0};
  options.struct_size = sizeof(::MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_FLAG_NONE;

  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult result = mojo::CreateMessagePipe(&options, &handle0, &handle1);

  // Create a result of the form blink::CreateMessagePipeResult, see
  // third_party/blink/renderer/core/mojo/mojo_create_message_pipe_result.idl.
  gin::DataObjectBuilder v8_result_dict(isolate);
  v8_result_dict.Set("result", result);

  if (result == MOJO_RESULT_OK) {
    v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
    v8_result_dict
        .Set("handle0", MojoHandle::Create(context, mojo::ScopedHandle::From(
                                                        std::move(handle0))))
        .Set("handle1", MojoHandle::Create(context, mojo::ScopedHandle::From(
                                                        std::move(handle1))));
  }

  arguments->Return(v8_result_dict.Build());
}

void Mojo::BindInterface(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  // The arguments are defined in third_party/blink/renderer/core/mojo/mojo.idl:
  // static void bindInterface(DOMString interfaceName, MojoHandle
  // request_handle, optional MojoScope scope = "context"); In this
  // implementation, the MojoScope is unused.
  v8::LocalVector<v8::Value> args = arguments->GetAll();
  v8::Local<v8::String> v8_interface_name = args[0].As<v8::String>();
  std::string interface_name;
  gin::ConvertFromV8(isolate, v8_interface_name, &interface_name);

  gin::Handle<MojoHandle> gin_handle;
  if (!gin::ConvertFromV8(isolate, args[1], &gin_handle)) {
    LOG(ERROR) << "Failed to get handle from Mojo::BindInterface";
    return;
  }
  auto handle = mojo::ScopedMessagePipeHandle::From(gin_handle->TakeHandle());

  mojo::GenericPendingReceiver receiver(interface_name, std::move(handle));

  // Use the context's global object to get the pointer to the V8 manager in
  // order to bind this receiver.
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
  CHECK(!context.IsEmpty());
  V8Environment::GetFromContext(context)->BindInterface(interface_name,
                                                        std::move(receiver));
}

Mojo::Mojo(v8::Local<v8::Context> context) : RegisteredWrappable(context) {}

}  // namespace ax
