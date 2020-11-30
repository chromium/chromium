// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_handle.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_offscreen_canvas.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_transform_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// The "Blink-side" serialization version, which defines how Blink will behave
// during the serialization process, is in
// SerializedScriptValue::wireFormatVersion. The serialization format has two
// "envelopes": an outer one controlled by Blink and an inner one by V8.
//
// They are formatted as follows:
// [version tag] [Blink version] [version tag] [v8 version] ...
//
// Before version 16, there was only a single envelope and the version number
// for both parts was always equal.
//
// See also V8ScriptValueDeserializer.cpp.
//
// This version number must be incremented whenever any incompatible changes are
// made to how Blink writes data. Purely V8-side changes do not require an
// adjustment to this value.

V8ScriptValueSerializer::V8ScriptValueSerializer(ScriptState* script_state,
                                                 const Options& options)
    : script_state_(script_state),
      serialized_script_value_(SerializedScriptValue::Create()),
      serializer_(script_state_->GetIsolate(), this),
      transferables_(options.transferables),
      blob_info_array_(options.blob_info),
      wasm_policy_(options.wasm_policy),
      for_storage_(options.for_storage == SerializedScriptValue::kForStorage) {}

scoped_refptr<SerializedScriptValue> V8ScriptValueSerializer::Serialize(
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
#if DCHECK_IS_ON()
  DCHECK(!serialize_invoked_);
  serialize_invoked_ = true;
#endif
  DCHECK(serialized_script_value_);
  base::AutoReset<const ExceptionState*> reset(&exception_state_,
                                               &exception_state);

  // Prepare to transfer the provided transferables.
  PrepareTransfer(exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Write out the file header.
  WriteTag(kVersionTag);
  WriteUint32(SerializedScriptValue::kWireFormatVersion);
  serializer_.WriteHeader();

  // Serialize the value and handle errors.
  v8::TryCatch try_catch(script_state_->GetIsolate());
  bool wrote_value;
  if (!serializer_.WriteValue(script_state_->GetContext(), value)
           .To(&wrote_value)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  DCHECK(wrote_value);

  // Finalize the transfer (e.g. detaching array buffers).
  FinalizeTransfer(exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (shared_array_buffers_.size()) {
    auto* execution_context = ExecutionContext::From(script_state_);
    if (!execution_context->SharedArrayBufferTransferAllowed()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "SharedArrayBuffer transfer requires self.crossOriginIsolated.");
      return nullptr;
    }
  }

  serialized_script_value_->CloneSharedArrayBuffers(shared_array_buffers_);

  // Finalize the results.
  std::pair<uint8_t*, size_t> buffer = serializer_.Release();
  serialized_script_value_->SetData(
      SerializedScriptValue::DataBufferPtr(buffer.first), buffer.second);
  return std::move(serialized_script_value_);
}

void V8ScriptValueSerializer::PrepareTransfer(ExceptionState& exception_state) {
  if (!transferables_)
    return;

  // Transfer array buffers.
  for (uint32_t i = 0; i < transferables_->array_buffers.size(); i++) {
    DOMArrayBufferBase* array_buffer = transferables_->array_buffers[i].Get();
    if (!array_buffer->IsShared()) {
      v8::Local<v8::Value> wrapper = ToV8(array_buffer, script_state_);
      serializer_.TransferArrayBuffer(
          i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "SharedArrayBuffer can not be in transfer list.");
      return;
    }
  }
}

void V8ScriptValueSerializer::FinalizeTransfer(
    ExceptionState& exception_state) {
  // TODO(jbroman): Strictly speaking, this is not correct; transfer should
  // occur in the order of the transfer list.
  // https://html.spec.whatwg.org/C/#structuredclonewithtransfer

  v8::Isolate* isolate = script_state_->GetIsolate();

  ArrayBufferArray array_buffers;
  // The scope object to promptly free the backing store to avoid memory
  // regressions.
  // TODO(bikineev): Revisit after young generation is there.
  struct PromptlyFreeArrayBuffers {
    // The void* is to avoid blink-gc-plugin error.
    void* buffer;
    ~PromptlyFreeArrayBuffers() {
      static_cast<ArrayBufferArray*>(buffer)->clear();
    }
  } promptly_free_array_buffers{&array_buffers};
  if (transferables_)
    array_buffers.AppendVector(transferables_->array_buffers);

  if (!array_buffers.IsEmpty()) {
    serialized_script_value_->TransferArrayBuffers(isolate, array_buffers,
                                                   exception_state);
    if (exception_state.HadException())
      return;
  }

  if (transferables_) {
    serialized_script_value_->TransferImageBitmaps(
        isolate, transferables_->image_bitmaps, exception_state);
    if (exception_state.HadException())
      return;

    serialized_script_value_->TransferOffscreenCanvas(
        isolate, transferables_->offscreen_canvases, exception_state);
    if (exception_state.HadException())
      return;

    if (TransferableStreamsEnabled()) {
      // Order matters here, because the order in which streams are added to the
      // |stream_ports_| array must match the indexes which are calculated in
      // WriteDOMObject().
      serialized_script_value_->TransferReadableStreams(
          script_state_, transferables_->readable_streams, exception_state);
      if (exception_state.HadException())
        return;
      serialized_script_value_->TransferWritableStreams(
          script_state_, transferables_->writable_streams, exception_state);
      if (exception_state.HadException())
        return;
      serialized_script_value_->TransferTransformStreams(
          script_state_, transferables_->transform_streams, exception_state);
      if (exception_state.HadException())
        return;
    }
  }
}

void V8ScriptValueSerializer::WriteUTF8String(const String& string) {
  // TODO(jbroman): Ideally this method would take a WTF::StringView, but the
  // StringUTF8Adaptor trick doesn't yet work with StringView.
  StringUTF8Adaptor utf8(string);
  WriteUint32(utf8.size());
  WriteRawBytes(utf8.data(), utf8.size());
}

bool V8ScriptValueSerializer::WriteDOMObject(ScriptWrappable* wrappable,
                                             ExceptionState& exception_state) {
  const WrapperTypeInfo* wrapper_type_info = wrappable->GetWrapperTypeInfo();
  if (wrapper_type_info == V8Blob::GetWrapperTypeInfo()) {
    Blob* blob = wrappable->ToImpl<Blob>();
    serialized_script_value_->BlobDataHandles().Set(blob->Uuid(),
                                                    blob->GetBlobDataHandle());
    if (blob_info_array_) {
      size_t index = blob_info_array_->size();
      DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
      blob_info_array_->emplace_back(blob->GetBlobDataHandle(), blob->type(),
                                     blob->size());
      WriteTag(kBlobIndexTag);
      WriteUint32(static_cast<uint32_t>(index));
    } else {
      WriteTag(kBlobTag);
      WriteUTF8String(blob->Uuid());
      WriteUTF8String(blob->type());
      WriteUint64(blob->size());
    }
    return true;
  }
  if (wrapper_type_info == V8File::GetWrapperTypeInfo()) {
    WriteTag(blob_info_array_ ? kFileIndexTag : kFileTag);
    return WriteFile(wrappable->ToImpl<File>(), exception_state);
  }
  if (wrapper_type_info == V8FileList::GetWrapperTypeInfo()) {
    // This does not presently deduplicate a File object and its entry in a
    // FileList, which is non-standard behavior.
    FileList* file_list = wrappable->ToImpl<FileList>();
    unsigned length = file_list->length();
    WriteTag(blob_info_array_ ? kFileListIndexTag : kFileListTag);
    WriteUint32(length);
    for (unsigned i = 0; i < length; i++) {
      if (!WriteFile(file_list->item(i), exception_state))
        return false;
    }
    return true;
  }
  if (wrapper_type_info == V8ImageBitmap::GetWrapperTypeInfo()) {
    ImageBitmap* image_bitmap = wrappable->ToImpl<ImageBitmap>();
    if (image_bitmap->IsNeutered()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An ImageBitmap is detached and could not be cloned.");
      return false;
    }

    auto* execution_context = ExecutionContext::From(script_state_);
    // If this ImageBitmap was transferred, it can be serialized by index.
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->image_bitmaps.Find(image_bitmap);
    if (index != kNotFound) {
      if (image_bitmap->OriginClean()) {
        execution_context->CountUse(
            mojom::WebFeature::kOriginCleanImageBitmapTransfer);
      } else {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataCloneError,
            "Non-origin-clean ImageBitmap cannot be transferred.");
        return false;
      }

      DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
      WriteTag(kImageBitmapTransferTag);
      WriteUint32(static_cast<uint32_t>(index));
      return true;
    }

    // Otherwise, it must be fully serialized.
    if (image_bitmap->OriginClean()) {
      execution_context->CountUse(
          mojom::WebFeature::kOriginCleanImageBitmapSerialization);
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Non-origin-clean ImageBitmap cannot be cloned.");
      return false;
    }
    WriteTag(kImageBitmapTag);
    SerializedColorParams color_params(image_bitmap->GetCanvasColorParams());
    WriteUint32Enum(ImageSerializationTag::kCanvasColorSpaceTag);
    WriteUint32Enum(color_params.GetSerializedColorSpace());
    WriteUint32Enum(ImageSerializationTag::kCanvasPixelFormatTag);
    WriteUint32Enum(color_params.GetSerializedPixelFormat());
    WriteUint32Enum(ImageSerializationTag::kCanvasOpacityModeTag);
    WriteUint32Enum(color_params.GetSerializedOpacityMode());
    WriteUint32Enum(ImageSerializationTag::kOriginCleanTag);
    WriteUint32(image_bitmap->OriginClean());
    WriteUint32Enum(ImageSerializationTag::kIsPremultipliedTag);
    WriteUint32(image_bitmap->IsPremultiplied());
    WriteUint32Enum(ImageSerializationTag::kEndTag);
    WriteUint32(image_bitmap->width());
    WriteUint32(image_bitmap->height());
    Vector<uint8_t> pixels = image_bitmap->CopyBitmapData();
    // Check if we succeeded to copy the BitmapData.
    if (image_bitmap->width() != 0 && image_bitmap->height() != 0 &&
        pixels.size() == 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An ImageBitmap could not be read successfully.");
      return false;
    }
    WriteUint32(pixels.size());
    WriteRawBytes(pixels.data(), pixels.size());
    return true;
  }
  if (wrapper_type_info == V8ImageData::GetWrapperTypeInfo()) {
    ImageData* image_data = wrappable->ToImpl<ImageData>();
    WriteTag(kImageDataTag);
    SerializedColorParams color_params(image_data->GetCanvasColorSpace(),
                                       image_data->GetImageDataStorageFormat());
    WriteUint32Enum(ImageSerializationTag::kCanvasColorSpaceTag);
    WriteUint32Enum(color_params.GetSerializedColorSpace());
    WriteUint32Enum(ImageSerializationTag::kImageDataStorageFormatTag);
    WriteUint32Enum(color_params.GetSerializedImageDataStorageFormat());
    WriteUint32Enum(ImageSerializationTag::kEndTag);
    WriteUint32(image_data->width());
    WriteUint32(image_data->height());
    DOMArrayBufferBase* pixel_buffer = image_data->BufferBase();
    size_t pixel_buffer_length = pixel_buffer->ByteLength();
    WriteUint64(base::strict_cast<uint64_t>(pixel_buffer_length));
    WriteRawBytes(pixel_buffer->Data(), pixel_buffer_length);
    return true;
  }
  if (wrapper_type_info == V8DOMPoint::GetWrapperTypeInfo()) {
    DOMPoint* point = wrappable->ToImpl<DOMPoint>();
    WriteTag(kDOMPointTag);
    WriteDouble(point->x());
    WriteDouble(point->y());
    WriteDouble(point->z());
    WriteDouble(point->w());
    return true;
  }
  if (wrapper_type_info == V8DOMPointReadOnly::GetWrapperTypeInfo()) {
    DOMPointReadOnly* point = wrappable->ToImpl<DOMPointReadOnly>();
    WriteTag(kDOMPointReadOnlyTag);
    WriteDouble(point->x());
    WriteDouble(point->y());
    WriteDouble(point->z());
    WriteDouble(point->w());
    return true;
  }
  if (wrapper_type_info == V8DOMRect::GetWrapperTypeInfo()) {
    DOMRect* rect = wrappable->ToImpl<DOMRect>();
    WriteTag(kDOMRectTag);
    WriteDouble(rect->x());
    WriteDouble(rect->y());
    WriteDouble(rect->width());
    WriteDouble(rect->height());
    return true;
  }
  if (wrapper_type_info == V8DOMRectReadOnly::GetWrapperTypeInfo()) {
    DOMRectReadOnly* rect = wrappable->ToImpl<DOMRectReadOnly>();
    WriteTag(kDOMRectReadOnlyTag);
    WriteDouble(rect->x());
    WriteDouble(rect->y());
    WriteDouble(rect->width());
    WriteDouble(rect->height());
    return true;
  }
  if (wrapper_type_info == V8DOMQuad::GetWrapperTypeInfo()) {
    DOMQuad* quad = wrappable->ToImpl<DOMQuad>();
    WriteTag(kDOMQuadTag);
    for (const DOMPoint* point :
         {quad->p1(), quad->p2(), quad->p3(), quad->p4()}) {
      WriteDouble(point->x());
      WriteDouble(point->y());
      WriteDouble(point->z());
      WriteDouble(point->w());
    }
    return true;
  }
  if (wrapper_type_info == V8DOMMatrix::GetWrapperTypeInfo()) {
    DOMMatrix* matrix = wrappable->ToImpl<DOMMatrix>();
    if (matrix->is2D()) {
      WriteTag(kDOMMatrix2DTag);
      WriteDouble(matrix->a());
      WriteDouble(matrix->b());
      WriteDouble(matrix->c());
      WriteDouble(matrix->d());
      WriteDouble(matrix->e());
      WriteDouble(matrix->f());
    } else {
      WriteTag(kDOMMatrixTag);
      WriteDouble(matrix->m11());
      WriteDouble(matrix->m12());
      WriteDouble(matrix->m13());
      WriteDouble(matrix->m14());
      WriteDouble(matrix->m21());
      WriteDouble(matrix->m22());
      WriteDouble(matrix->m23());
      WriteDouble(matrix->m24());
      WriteDouble(matrix->m31());
      WriteDouble(matrix->m32());
      WriteDouble(matrix->m33());
      WriteDouble(matrix->m34());
      WriteDouble(matrix->m41());
      WriteDouble(matrix->m42());
      WriteDouble(matrix->m43());
      WriteDouble(matrix->m44());
    }
    return true;
  }
  if (wrapper_type_info == V8DOMMatrixReadOnly::GetWrapperTypeInfo()) {
    DOMMatrixReadOnly* matrix = wrappable->ToImpl<DOMMatrixReadOnly>();
    if (matrix->is2D()) {
      WriteTag(kDOMMatrix2DReadOnlyTag);
      WriteDouble(matrix->a());
      WriteDouble(matrix->b());
      WriteDouble(matrix->c());
      WriteDouble(matrix->d());
      WriteDouble(matrix->e());
      WriteDouble(matrix->f());
    } else {
      WriteTag(kDOMMatrixReadOnlyTag);
      WriteDouble(matrix->m11());
      WriteDouble(matrix->m12());
      WriteDouble(matrix->m13());
      WriteDouble(matrix->m14());
      WriteDouble(matrix->m21());
      WriteDouble(matrix->m22());
      WriteDouble(matrix->m23());
      WriteDouble(matrix->m24());
      WriteDouble(matrix->m31());
      WriteDouble(matrix->m32());
      WriteDouble(matrix->m33());
      WriteDouble(matrix->m34());
      WriteDouble(matrix->m41());
      WriteDouble(matrix->m42());
      WriteDouble(matrix->m43());
      WriteDouble(matrix->m44());
    }
    return true;
  }
  if (wrapper_type_info == V8MessagePort::GetWrapperTypeInfo()) {
    MessagePort* message_port = wrappable->ToImpl<MessagePort>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->message_ports.Find(message_port);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A MessagePort could not be cloned because it was not transferred.");
      return false;
    }
    DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
    WriteTag(kMessagePortTag);
    WriteUint32(static_cast<uint32_t>(index));
    return true;
  }
  if (wrapper_type_info == V8MojoHandle::GetWrapperTypeInfo() &&
      RuntimeEnabledFeatures::MojoJSEnabled()) {
    MojoHandle* mojo_handle = wrappable->ToImpl<MojoHandle>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->mojo_handles.Find(mojo_handle);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A MojoHandle could not be cloned because it was not transferred.");
      return false;
    }
    DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
    serialized_script_value_->MojoHandles().push_back(
        mojo_handle->TakeHandle());
    index = serialized_script_value_->MojoHandles().size() - 1;
    WriteTag(kMojoHandleTag);
    WriteUint32(static_cast<uint32_t>(index));
    return true;
  }
  if (wrapper_type_info == V8OffscreenCanvas::GetWrapperTypeInfo()) {
    OffscreenCanvas* canvas = wrappable->ToImpl<OffscreenCanvas>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->offscreen_canvases.Find(canvas);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An OffscreenCanvas could not be cloned "
          "because it was not transferred.");
      return false;
    }
    if (canvas->IsNeutered()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An OffscreenCanvas could not be cloned because it was detached.");
      return false;
    }
    if (canvas->RenderingContext()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An OffscreenCanvas could not be cloned "
          "because it had a rendering context.");
      return false;
    }
    WriteTag(kOffscreenCanvasTransferTag);
    WriteUint32(canvas->width());
    WriteUint32(canvas->height());
    WriteUint64(canvas->PlaceholderCanvasId());
    WriteUint32(canvas->ClientId());
    WriteUint32(canvas->SinkId());
    WriteUint32(canvas->FilterQuality() == kNone_SkFilterQuality ? 0 : 1);
    return true;
  }
  if (wrapper_type_info == V8ReadableStream::GetWrapperTypeInfo() &&
      TransferableStreamsEnabled()) {
    ReadableStream* stream = wrappable->ToImpl<ReadableStream>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->readable_streams.Find(stream);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A ReadableStream could not be cloned "
                                        "because it was not transferred.");
      return false;
    }
    if (stream->IsLocked()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A ReadableStream could not be cloned because it was locked");
      return false;
    }
    WriteTag(kReadableStreamTransferTag);
    WriteUint32(static_cast<uint32_t>(index));
    return true;
  }
  if (wrapper_type_info == V8WritableStream::GetWrapperTypeInfo() &&
      TransferableStreamsEnabled()) {
    WritableStream* stream = wrappable->ToImpl<WritableStream>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->writable_streams.Find(stream);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A WritableStream could not be cloned "
                                        "because it was not transferred.");
      return false;
    }
    if (stream->locked()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A WritableStream could not be cloned because it was locked");
      return false;
    }
    WriteTag(kWritableStreamTransferTag);
    DCHECK(transferables_);
    // The index calculation depends on the order that TransferReadableStreams
    // and TransferWritableStreams are called in
    // V8ScriptValueSerializer::FinalizeTransfer.
    WriteUint32(
        static_cast<uint32_t>(index + transferables_->readable_streams.size()));
    return true;
  }
  if (wrapper_type_info == V8TransformStream::GetWrapperTypeInfo() &&
      TransferableStreamsEnabled()) {
    TransformStream* stream = wrappable->ToImpl<TransformStream>();
    size_t index = kNotFound;
    if (transferables_)
      index = transferables_->transform_streams.Find(stream);
    if (index == kNotFound) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A TransformStream could not be cloned "
                                        "because it was not transferred.");
      return false;
    }
    // https://streams.spec.whatwg.org/#ts-transfer
    // 3. If ! IsReadableStreamLocked(readable) is true, throw a
    //    "DataCloneError" DOMException.
    // 4. If ! IsWritableStreamLocked(writable) is true, throw a
    //    "DataCloneError" DOMException.
    if (stream->Readable()->locked() || stream->Writable()->locked()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A TransformStream could not be cloned because it was locked");
      return false;
    }
    WriteTag(kTransformStreamTransferTag);
    DCHECK(transferables_);
    // TransformStreams use two ports each. The stored index is the index of the
    // first one. The first TransformStream is stored in the array after all the
    // ReadableStreams and WritableStreams.
    WriteUint32(static_cast<uint32_t>(index * 2 +
                                      transferables_->readable_streams.size() +
                                      transferables_->writable_streams.size()));
    return true;
  }
  if (wrapper_type_info == V8DOMException::GetWrapperTypeInfo()) {
    DOMException* exception = wrappable->ToImpl<DOMException>();
    WriteTag(kDOMExceptionTag);
    WriteUTF8String(exception->name());
    WriteUTF8String(exception->message());
    // We may serialize the stack property in the future, so we store a null
    // string in order to avoid future scheme changes.
    String stack_unused;
    WriteUTF8String(stack_unused);
    return true;
  }
  return false;
}

bool V8ScriptValueSerializer::WriteFile(File* file,
                                        ExceptionState& exception_state) {
  serialized_script_value_->BlobDataHandles().Set(file->Uuid(),
                                                  file->GetBlobDataHandle());
  if (blob_info_array_) {
    size_t index = blob_info_array_->size();
    DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
    blob_info_array_->emplace_back(
        file->GetBlobDataHandle(), file->name(), file->type(),
        file->LastModifiedTimeForSerialization(), file->size());
    WriteUint32(static_cast<uint32_t>(index));
  } else {
    WriteUTF8String(file->HasBackingFile() ? file->GetPath() : g_empty_string);
    WriteUTF8String(file->name());
    WriteUTF8String(file->webkitRelativePath());
    WriteUTF8String(file->Uuid());
    WriteUTF8String(file->type());
    // Historically we sometimes wouldn't write metadata. This next integer was
    // 1 or 0 to indicate if metadata is present. Now we always write metadata,
    // hence always have this hardcoded 1.
    WriteUint32(1);
    WriteUint64(file->size());
    base::Optional<base::Time> last_modified =
        file->LastModifiedTimeForSerialization();
    WriteDouble(last_modified ? last_modified->ToJsTimeIgnoringNull()
                              : std::numeric_limits<double>::quiet_NaN());
    WriteUint32(file->GetUserVisibility() == File::kIsUserVisible ? 1 : 0);
  }
  return true;
}

void V8ScriptValueSerializer::ThrowDataCloneError(
    v8::Local<v8::String> v8_message) {
  DCHECK(exception_state_);
  ExceptionState exception_state(
      script_state_->GetIsolate(), exception_state_->Context(),
      exception_state_->InterfaceName(), exception_state_->PropertyName());
  exception_state.ThrowDOMException(
      DOMExceptionCode::kDataCloneError,
      ToBlinkString<String>(v8_message, kDoNotExternalize));
}

v8::Maybe<bool> V8ScriptValueSerializer::WriteHostObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> object) {
  DCHECK(exception_state_);
  DCHECK_EQ(isolate, script_state_->GetIsolate());
  ExceptionState exception_state(isolate, exception_state_->Context(),
                                 exception_state_->InterfaceName(),
                                 exception_state_->PropertyName());

  if (!V8DOMWrapper::IsWrapper(isolate, object)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "An object could not be cloned.");
    return v8::Nothing<bool>();
  }
  ScriptWrappable* wrappable = ToScriptWrappable(object);
  bool wrote_dom_object = WriteDOMObject(wrappable, exception_state);
  if (wrote_dom_object) {
    DCHECK(!exception_state.HadException());
    return v8::Just(true);
  }
  if (!exception_state.HadException()) {
    StringView interface = wrappable->GetWrapperTypeInfo()->interface_name;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        interface + " object could not be cloned.");
  }
  return v8::Nothing<bool>();
}

v8::Maybe<uint32_t> V8ScriptValueSerializer::GetSharedArrayBufferId(
    v8::Isolate* isolate,
    v8::Local<v8::SharedArrayBuffer> v8_shared_array_buffer) {
  if (for_storage_) {
    DCHECK(exception_state_);
    DCHECK_EQ(isolate, script_state_->GetIsolate());
    ExceptionState exception_state(isolate, exception_state_->Context(),
                                   exception_state_->InterfaceName(),
                                   exception_state_->PropertyName());
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        "A SharedArrayBuffer can not be serialized for storage.");
    return v8::Nothing<uint32_t>();
  }

  DOMSharedArrayBuffer* shared_array_buffer =
      V8SharedArrayBuffer::ToImpl(v8_shared_array_buffer);

  // The index returned from this function will be serialized into the data
  // stream. When deserializing, this will be used to index into the
  // sharedArrayBufferContents array of the SerializedScriptValue.
  uint32_t index = shared_array_buffers_.Find(shared_array_buffer);
  if (index == kNotFound) {
    shared_array_buffers_.push_back(shared_array_buffer);
    index = shared_array_buffers_.size() - 1;
  }
  return v8::Just<uint32_t>(index);
}

v8::Maybe<uint32_t> V8ScriptValueSerializer::GetWasmModuleTransferId(
    v8::Isolate* isolate,
    v8::Local<v8::WasmModuleObject> module) {
  if (for_storage_) {
    DCHECK(exception_state_);
    DCHECK_EQ(isolate, script_state_->GetIsolate());
    ExceptionState exception_state(isolate, exception_state_->Context(),
                                   exception_state_->InterfaceName(),
                                   exception_state_->PropertyName());
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        "A WebAssembly.Module can not be serialized for storage.");
    return v8::Nothing<uint32_t>();
  }

  switch (wasm_policy_) {
    case Options::kSerialize:
      return v8::Nothing<uint32_t>();

    case Options::kBlockedInNonSecureContext: {
      // This happens, currently, when we try to serialize to IndexedDB
      // in an non-secure context.
      ExceptionState exception_state(isolate, exception_state_->Context(),
                                     exception_state_->InterfaceName(),
                                     exception_state_->PropertyName());
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "Serializing WebAssembly modules in "
                                        "non-secure contexts is not allowed.");
      return v8::Nothing<uint32_t>();
    }

    case Options::kTransfer: {
      // We don't expect scenarios with numerous wasm modules being transferred
      // around. Most likely, we'll have one module. The vector approach is
      // simple and should perform sufficiently well under these expectations.
      serialized_script_value_->WasmModules().push_back(
          module->GetCompiledModule());
      uint32_t size =
          static_cast<uint32_t>(serialized_script_value_->WasmModules().size());
      DCHECK_GE(size, 1u);
      return v8::Just(size - 1);
    }

    case Options::kUnspecified:
      NOTREACHED();
  }
  return v8::Nothing<uint32_t>();
}

void* V8ScriptValueSerializer::ReallocateBufferMemory(void* old_buffer,
                                                      size_t size,
                                                      size_t* actual_size) {
  *actual_size = WTF::Partitions::BufferActualSize(size);
  return WTF::Partitions::BufferTryRealloc(old_buffer, *actual_size,
                                           "SerializedScriptValue buffer");
}

void V8ScriptValueSerializer::FreeBufferMemory(void* buffer) {
  return WTF::Partitions::BufferFree(buffer);
}

bool V8ScriptValueSerializer::TransferableStreamsEnabled() const {
  return RuntimeEnabledFeatures::TransferableStreamsEnabled(
      ExecutionContext::From(script_state_));
}

}  // namespace blink
