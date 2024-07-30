// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/accessibility/features/mojo/mojo_handle.h"

#include <memory>

#include "base/logging.h"
#include "base/types/fixed_array.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/public/context_holder.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/accessibility/features/mojo/mojo_watch_callback.h"
#include "services/accessibility/features/mojo/mojo_watcher.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace ax {

// static
gin::WrapperInfo MojoHandle::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
gin::Handle<MojoHandle> MojoHandle::Create(v8::Local<v8::Context> context,
                                           mojo::ScopedHandle handle) {
  return gin::CreateHandle(context->GetIsolate(),
                           new MojoHandle(context, std::move(handle)));
}

MojoHandle::~MojoHandle() = default;

gin::ObjectTemplateBuilder MojoHandle::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<MojoHandle>::GetObjectTemplateBuilder(isolate)
      .SetMethod("watch", &MojoHandle::Watch)
      .SetMethod("close", &MojoHandle::Close)
      .SetMethod("readMessage", &MojoHandle::ReadMessage)
      .SetMethod("writeMessage", &MojoHandle::WriteMessage);
}

void MojoHandle::Watch(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  v8::LocalVector<v8::Value> args = arguments->GetAll();
  CHECK_EQ(args.size(), 2u);

  // See third_party/blink/renderer/core/mojo/mojo_handle_signals.idl,
  // which defines the first argument as:
  // dictionary MojoHandleSignals {
  //   boolean readable = false;
  //   boolean writable = false;
  //   boolean peerClosed = false;
  // };
  CHECK(args[0]->IsObject());
  gin::Dictionary signals(isolate, args[0].As<v8::Object>());
  bool readable, writable, peer_closed = false;
  signals.Get("readable", &readable);
  signals.Get("writable", &writable);
  signals.Get("peerClosed", &peer_closed);

  // Now need to extract the MojoWatchCallback from the second
  // argument, see third_party/blink/renderer/core/mojo/mojo_handle.idl.
  CHECK(args[1]->IsFunction());
  v8::Local<v8::Function> v8_callback = args[1].As<v8::Function>();
  auto context_holder = std::make_unique<gin::ContextHolder>(isolate);
  context_holder->SetContext(context);
  auto callback = std::make_unique<MojoWatchCallback>(std::move(context_holder),
                                                      v8_callback);

  // Then make a MojoWatcher with the signals and callback, and return it.
  // The MojoWatcher should start watching when it's created
  // and call the callback then.
  arguments->Return(MojoWatcher::Create(context, handle_.get(), readable,
                                        writable, peer_closed,
                                        std::move(callback)));
}

void MojoHandle::Close(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  handle_.reset();
}

void MojoHandle::ReadMessage(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  // The result should be formatted as per
  // third_party/blink/renderer/core/mojo/mojo_read_message_result.idl:
  // dictionary MojoReadMessageResult {
  //   required MojoResult result;
  //   ArrayBuffer buffer;
  //   sequence<MojoHandle> handles;
  // };
  gin::DataObjectBuilder result_dict(isolate);

  mojo::ScopedMessageHandle message;
  MojoResult result =
      mojo::ReadMessageNew(mojo::MessagePipeHandle(handle_.get().value()),
                           &message, MOJO_READ_MESSAGE_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    result_dict.Set("result", result);
    arguments->Return(result_dict.Build());
    return;
  }

  result = MojoSerializeMessage(message->value(), nullptr);
  if (result != MOJO_RESULT_OK && result != MOJO_RESULT_FAILED_PRECONDITION) {
    result_dict.Set("result", MOJO_RESULT_ABORTED);
    arguments->Return(result_dict.Build());
    return;
  }

  uint32_t num_bytes = 0, num_handles = 0;
  void* bytes = nullptr;
  std::vector<::MojoHandle> raw_handles;
  result = MojoGetMessageData(message->value(), /*options=*/nullptr, &bytes,
                              &num_bytes, nullptr, &num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    // We don't know how many handles are in a message at first. The first call
    // retrieves that in num_handles. The second call can then provide adequate
    // storage for the copies.
    raw_handles.resize(num_handles);
    result = MojoGetMessageData(message->value(), /*options=*/nullptr, &bytes,
                                &num_bytes, raw_handles.data(), &num_handles);
  }

  if (result != MOJO_RESULT_OK) {
    result_dict.Set("result", MOJO_RESULT_ABORTED);
    arguments->Return(result_dict.Build());
    return;
  }

  void* buffer =
      gin::ArrayBufferAllocator::SharedInstance()->Allocate(num_bytes);
  auto deleter = [](void* buffer, size_t length, void* data) {
    gin::ArrayBufferAllocator::SharedInstance()->Free(buffer, length);
  };
  std::unique_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(buffer, num_bytes, deleter, nullptr);

  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, std::move(backing_store));
  if (num_bytes) {
    CHECK(array_buffer->Data());
    memcpy(array_buffer->Data(), bytes, num_bytes);
  }
  result_dict.Set("buffer", array_buffer);

  std::vector<gin::Handle<MojoHandle>> handles(num_handles);
  for (uint32_t i = 0; i < num_handles; ++i) {
    handles[i] = MojoHandle::Create(
        arguments->GetHolderCreationContext(),
        mojo::MakeScopedHandle(mojo::Handle(raw_handles[i])));
  }
  result_dict.Set("handles", handles);

  result_dict.Set("result", result);
  arguments->Return(result_dict.Build());
}

void MojoHandle::WriteMessage(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  // third_party/blink/renderer/core/mojo/mojo_handle.idl defines this
  // method as:
  // MojoResult writeMessage(BufferSource buffer, sequence<MojoHandle> handles);
  v8::LocalVector<v8::Value> args = arguments->GetAll();
  DCHECK_EQ(args.size(), 2u);
  DCHECK(args[0]->IsArrayBuffer() || args[0]->IsArrayBufferView());
  DCHECK(args[1]->IsArray());

  v8::Local<v8::Array> v8_handles = args[1].As<v8::Array>();

  std::vector<gin::Handle<MojoHandle>> handles;
  if (!gin::ConvertFromV8(isolate, v8_handles, &handles)) {
    arguments->Return(MOJO_RESULT_INVALID_ARGUMENT);
    return;
  }

  std::vector<mojo::ScopedHandle> scoped_handles;
  scoped_handles.reserve(handles.size());
  bool has_invalid_handles = false;
  for (auto& handle : handles) {
    if (!handle->handle_.is_valid()) {
      has_invalid_handles = true;
    } else {
      scoped_handles.emplace_back(std::move(handle->handle_));
    }
  }
  if (has_invalid_handles) {
    arguments->Return(MOJO_RESULT_INVALID_ARGUMENT);
    return;
  }

  base::span<const uint8_t> bytes;
  if (args[0]->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> array = args[0].As<v8::ArrayBuffer>();
    bytes = base::make_span(static_cast<const uint8_t*>(array->Data()),
                            array->ByteLength());
  } else {
    v8::Local<v8::ArrayBufferView> view = args[0].As<v8::ArrayBufferView>();
    base::FixedArray<uint8_t> bites(view->ByteLength());
    view->CopyContents(bites.data(), bites.size());
    bytes = base::span(bites);
  }

  auto message = mojo::Message(bytes, base::make_span(scoped_handles));
  DCHECK(!message.IsNull());
  MojoResult result = mojo::WriteMessageNew(
      mojo::MessagePipeHandle(handle_.get().value()), message.TakeMojoMessage(),
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  arguments->Return(result);
}

mojo::ScopedHandle MojoHandle::TakeHandle() {
  return std::move(handle_);
}

MojoHandle::MojoHandle(v8::Local<v8::Context> context,
                       mojo::ScopedHandle handle)
    : RegisteredWrappable(context), handle_(std::move(handle)) {}

}  // namespace ax
