// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"

#include "third_party/blink/public/mojom/messaging/user_activation_snapshot.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/window_post_message_options.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"

namespace blink {

scoped_refptr<SerializedScriptValue> PostMessageHelper::SerializeMessageByMove(
    v8::Isolate* isolate,
    const ScriptValue& message,
    const PostMessageOptions* options,
    Transferables& transferables,
    ExceptionState& exception_state) {
  if (options->hasTransfer() && !options->transfer().IsEmpty()) {
    if (!SerializedScriptValue::ExtractTransferables(
            isolate, options->transfer(), transferables, exception_state)) {
      return nullptr;
    }
  }

  SerializedScriptValue::SerializeOptions serialize_options;
  serialize_options.transferables = &transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      SerializedScriptValue::Serialize(isolate, message.V8Value(),
                                       serialize_options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  serialized_message->UnregisterMemoryAllocatedWithCurrentScriptContext();
  return serialized_message;
}

scoped_refptr<SerializedScriptValue> PostMessageHelper::SerializeMessageByCopy(
    v8::Isolate* isolate,
    const ScriptValue& message,
    const PostMessageOptions* options,
    Transferables& transferables,
    ExceptionState& exception_state) {
  if (options->hasTransfer() && !options->transfer().IsEmpty()) {
    if (!SerializedScriptValue::ExtractTransferables(
            isolate, options->transfer(), transferables, exception_state)) {
      return nullptr;
    }
  }

  // Copying the transferables by move semantics is not supported for the
  // caller of this function so emulate it by copy-and-neuter semantics
  // that sends array buffers and image
  // bitmaps via structured clone and then neuters the original objects.
  // Clear references to array buffers and image bitmaps from transferables
  // so that the serializer can consider the array buffers as
  // non-transferable and serialize them into the message.
  ArrayBufferArray transferable_array_buffers =
      SerializedScriptValue::ExtractNonSharedArrayBuffers(transferables);
  ImageBitmapArray transferable_image_bitmaps = transferables.image_bitmaps;
  transferables.image_bitmaps.clear();
  SerializedScriptValue::SerializeOptions serialize_options;
  serialize_options.transferables = &transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      SerializedScriptValue::Serialize(isolate, message.V8Value(),
                                       serialize_options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Detach the original array buffers on the sender context.
  SerializedScriptValue::TransferArrayBufferContents(
      isolate, transferable_array_buffers, exception_state);
  if (exception_state.HadException())
    return nullptr;
  // Neuter the original image bitmaps on the sender context.
  SerializedScriptValue::TransferImageBitmapContents(
      isolate, transferable_image_bitmaps, exception_state);
  if (exception_state.HadException())
    return nullptr;

  serialized_message->UnregisterMemoryAllocatedWithCurrentScriptContext();
  return serialized_message;
}

mojom::blink::UserActivationSnapshotPtr
PostMessageHelper::CreateUserActivationSnapshot(
    ExecutionContext* execution_context,
    const PostMessageOptions* options) {
  if (!options->includeUserActivation())
    return nullptr;
  if (LocalDOMWindow* dom_window = execution_context->ExecutingWindow()) {
    if (LocalFrame* frame = dom_window->GetFrame()) {
      return mojom::blink::UserActivationSnapshot::New(
          frame->HasBeenActivated(),
          LocalFrame::HasTransientUserActivation(frame));
    }
  }
  return nullptr;
}

// static
scoped_refptr<const SecurityOrigin> PostMessageHelper::GetTargetOrigin(
    const WindowPostMessageOptions* options,
    const Document& source_document,
    ExceptionState& exception_state) {
  const String& target_origin = options->targetOrigin();
  if (target_origin == "/")
    return source_document.GetSecurityOrigin();
  if (target_origin == "*")
    return nullptr;
  scoped_refptr<const SecurityOrigin> target =
      SecurityOrigin::CreateFromString(target_origin);
  // It doesn't make sense target a postMessage at an opaque origin
  // because there's no way to represent an opaque origin in a string.
  if (target->IsOpaque()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid target origin '" +
                                          target_origin +
                                          "' in a call to 'postMessage'.");
    return nullptr;
  }
  return target;
}

}  // namespace blink
