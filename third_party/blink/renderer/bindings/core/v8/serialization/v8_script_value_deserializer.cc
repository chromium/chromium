// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

// The "Blink-side" serialization version, which defines how Blink will behave
// during the serialization process. The serialization format has two
// "envelopes": an outer one controlled by Blink and an inner one by V8.
//
// They are formatted as follows:
// [version tag] [Blink version] [version tag] [v8 version] ...
//
// Before version 16, there was only a single envelope and the version number
// for both parts was always equal.
//
// See also V8ScriptValueDeserializer.cpp.
const uint32_t kMinVersionForSeparateEnvelope = 16;

// Returns the number of bytes consumed reading the Blink version envelope, and
// sets |*version| to the version. If no Blink envelope was detected, zero is
// returned.
size_t ReadVersionEnvelope(SerializedScriptValue* serialized_script_value,
                           uint32_t* out_version) {
  const uint8_t* raw_data = serialized_script_value->Data();
  const size_t length = serialized_script_value->DataLengthInBytes();
  if (!length || raw_data[0] != kVersionTag)
    return 0;

  // Read a 32-bit unsigned integer from varint encoding.
  uint32_t version = 0;
  size_t i = 1;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (i >= length)
      return 0;
    uint8_t byte = raw_data[i];
    if (LIKELY(shift < 32)) {
      version |= static_cast<uint32_t>(byte & 0x7f) << shift;
      shift += 7;
    }
    has_another_byte = byte & 0x80;
    i++;
  } while (has_another_byte);

  // If the version in the envelope is too low, this was not a Blink version
  // envelope.
  if (version < kMinVersionForSeparateEnvelope)
    return 0;

  // Otherwise, we did read the envelope. Hurray!
  *out_version = version;
  return i;
}

}  // namespace

V8ScriptValueDeserializer::V8ScriptValueDeserializer(
    ScriptState* script_state,
    UnpackedSerializedScriptValue* unpacked_value,
    const Options& options)
    : V8ScriptValueDeserializer(script_state,
                                unpacked_value,
                                unpacked_value->Value(),
                                options) {}

V8ScriptValueDeserializer::V8ScriptValueDeserializer(
    ScriptState* script_state,
    scoped_refptr<SerializedScriptValue> value,
    const Options& options)
    : V8ScriptValueDeserializer(script_state,
                                nullptr,
                                std::move(value),
                                options) {
  DCHECK(!serialized_script_value_->HasPackedContents())
      << "If the provided SerializedScriptValue could contain packed contents "
         "due to transfer, then it must be unpacked before deserialization. "
         "See SerializedScriptValue::Unpack.";
}

V8ScriptValueDeserializer::V8ScriptValueDeserializer(
    ScriptState* script_state,
    UnpackedSerializedScriptValue* unpacked_value,
    scoped_refptr<SerializedScriptValue> value,
    const Options& options)
    : script_state_(script_state),
      unpacked_value_(unpacked_value),
      serialized_script_value_(value),
      deserializer_(script_state_->GetIsolate(),
                    serialized_script_value_->Data(),
                    serialized_script_value_->DataLengthInBytes(),
                    this),
      transferred_message_ports_(options.message_ports),
      blob_info_array_(options.blob_info) {
  deserializer_.SetSupportsLegacyWireFormat(true);
  deserializer_.SetExpectInlineWasm(options.read_wasm_from_stream);
}

v8::Local<v8::Value> V8ScriptValueDeserializer::Deserialize() {
#if DCHECK_IS_ON()
  DCHECK(!deserialize_invoked_);
  deserialize_invoked_ = true;
#endif

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state_->GetContext();

  size_t version_envelope_size =
      ReadVersionEnvelope(serialized_script_value_.get(), &version_);
  if (version_envelope_size) {
    const void* blink_envelope;
    bool read_envelope = ReadRawBytes(version_envelope_size, &blink_envelope);
    DCHECK(read_envelope);
    DCHECK_GE(version_, kMinVersionForSeparateEnvelope);
  } else {
    DCHECK_EQ(version_, 0u);
  }

  bool read_header;
  if (!deserializer_.ReadHeader(context).To(&read_header))
    return v8::Null(isolate);
  DCHECK(read_header);

  // If there was no Blink envelope earlier, Blink shares the wire format
  // version from the V8 header.
  if (!version_)
    version_ = deserializer_.GetWireFormatVersion();

  // Prepare to transfer the provided transferables.
  Transfer();

  v8::Local<v8::Value> value;
  if (!deserializer_.ReadValue(context).ToLocal(&value))
    return v8::Null(isolate);
  return scope.Escape(value);
}

void V8ScriptValueDeserializer::Transfer() {
  // Thre's nothing to transfer if the deserializer was not given an unpacked
  // value.
  if (!unpacked_value_)
    return;

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Object> creation_context = context->Global();

  // Transfer array buffers.
  const auto& array_buffers = unpacked_value_->ArrayBuffers();
  for (unsigned i = 0; i < array_buffers.size(); i++) {
    DOMArrayBufferBase* array_buffer = array_buffers.at(i);
    v8::Local<v8::Value> wrapper =
        ToV8(array_buffer, creation_context, isolate);
    if (array_buffer->IsShared()) {
      DCHECK(wrapper->IsSharedArrayBuffer());
      deserializer_.TransferSharedArrayBuffer(
          i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
    } else {
      DCHECK(wrapper->IsArrayBuffer());
      deserializer_.TransferArrayBuffer(
          i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
  }
}

bool V8ScriptValueDeserializer::ReadUTF8String(String* string) {
  uint32_t utf8_length = 0;
  const void* utf8_data = nullptr;
  if (!ReadUint32(&utf8_length) || !ReadRawBytes(utf8_length, &utf8_data))
    return false;
  *string =
      String::FromUTF8(reinterpret_cast<const LChar*>(utf8_data), utf8_length);
  return true;
}

ScriptWrappable* V8ScriptValueDeserializer::ReadDOMObject(
    SerializationTag tag) {
  switch (tag) {
    case kBlobTag: {
      if (Version() < 3)
        return nullptr;
      String uuid, type;
      uint64_t size;
      if (!ReadUTF8String(&uuid) || !ReadUTF8String(&type) ||
          !ReadUint64(&size))
        return nullptr;
      auto blob_handle = GetOrCreateBlobDataHandle(uuid, type, size);
      if (!blob_handle)
        return nullptr;
      return Blob::Create(std::move(blob_handle));
    }
    case kBlobIndexTag: {
      if (Version() < 6 || !blob_info_array_)
        return nullptr;
      uint32_t index = 0;
      if (!ReadUint32(&index) || index >= blob_info_array_->size())
        return nullptr;
      const WebBlobInfo& info = (*blob_info_array_)[index];
      auto blob_handle = info.GetBlobHandle();
      if (!blob_handle) {
        blob_handle =
            GetOrCreateBlobDataHandle(info.Uuid(), info.GetType(), info.size());
      }
      if (!blob_handle)
        return nullptr;
      return Blob::Create(std::move(blob_handle));
    }
    case kFileTag:
      return ReadFile();
    case kFileIndexTag:
      return ReadFileIndex();
    case kFileListTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!ReadUint32(&length))
        return nullptr;
      FileList* file_list = FileList::Create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = ReadFile())
          file_list->Append(file);
        else
          return nullptr;
      }
      return file_list;
    }
    case kFileListIndexTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!ReadUint32(&length))
        return nullptr;
      FileList* file_list = FileList::Create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = ReadFileIndex())
          file_list->Append(file);
        else
          return nullptr;
      }
      return file_list;
    }
    case kImageBitmapTag: {
      SerializedColorSpace canvas_color_space = SerializedColorSpace::kSRGB;
      SerializedPixelFormat canvas_pixel_format = SerializedPixelFormat::kRGBA8;
      SerializedOpacityMode canvas_opacity_mode =
          SerializedOpacityMode::kOpaque;
      uint32_t origin_clean = 0, is_premultiplied = 0, width = 0, height = 0,
               byte_length = 0;
      const void* pixels = nullptr;
      if (Version() >= 18) {
        // read the list of key pair values for color settings, etc.
        bool is_done = false;
        do {
          ImageSerializationTag image_tag;
          if (!ReadUint32Enum<ImageSerializationTag>(&image_tag))
            return nullptr;
          switch (image_tag) {
            case ImageSerializationTag::kEndTag:
              is_done = true;
              break;
            case ImageSerializationTag::kCanvasColorSpaceTag:
              if (!ReadUint32Enum<SerializedColorSpace>(&canvas_color_space))
                return nullptr;
              break;
            case ImageSerializationTag::kCanvasPixelFormatTag:
              if (!ReadUint32Enum<SerializedPixelFormat>(&canvas_pixel_format))
                return nullptr;
              break;
            case ImageSerializationTag::kCanvasOpacityModeTag:
              if (!ReadUint32Enum<SerializedOpacityMode>(&canvas_opacity_mode))
                return nullptr;
              break;
            case ImageSerializationTag::kOriginCleanTag:
              if (!ReadUint32(&origin_clean) || origin_clean > 1)
                return nullptr;
              break;
            case ImageSerializationTag::kIsPremultipliedTag:
              if (!ReadUint32(&is_premultiplied) || is_premultiplied > 1)
                return nullptr;
              break;
            default:
              NOTREACHED();
          }
        } while (!is_done);
      } else if (!ReadUint32(&origin_clean) || origin_clean > 1 ||
                 !ReadUint32(&is_premultiplied) || is_premultiplied > 1) {
        return nullptr;
      }
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&byte_length) || !ReadRawBytes(byte_length, &pixels))
        return nullptr;
      CanvasColorParams color_params =
          SerializedColorParams(canvas_color_space, canvas_pixel_format,
                                canvas_opacity_mode,
                                SerializedImageDataStorageFormat::kUint8Clamped)
              .GetCanvasColorParams();
      base::CheckedNumeric<uint32_t> computed_byte_length = width;
      computed_byte_length *= height;
      computed_byte_length *= color_params.BytesPerPixel();
      if (!computed_byte_length.IsValid() ||
          computed_byte_length.ValueOrDie() != byte_length)
        return nullptr;
      return ImageBitmap::Create(pixels, width, height, is_premultiplied,
                                 origin_clean, color_params);
    }
    case kImageBitmapTransferTag: {
      uint32_t index = 0;
      if (!unpacked_value_)
        return nullptr;
      const auto& transferred_image_bitmaps = unpacked_value_->ImageBitmaps();
      if (!ReadUint32(&index) || index >= transferred_image_bitmaps.size())
        return nullptr;
      return transferred_image_bitmaps[index].Get();
    }
    case kImageDataTag: {
      SerializedColorSpace canvas_color_space = SerializedColorSpace::kSRGB;
      SerializedImageDataStorageFormat image_data_storage_format =
          SerializedImageDataStorageFormat::kUint8Clamped;
      uint32_t width = 0, height = 0, byte_length = 0;
      const void* pixels = nullptr;
      if (Version() >= 18) {
        bool is_done = false;
        do {
          ImageSerializationTag image_tag;
          if (!ReadUint32Enum<ImageSerializationTag>(&image_tag))
            return nullptr;
          switch (image_tag) {
            case ImageSerializationTag::kEndTag:
              is_done = true;
              break;
            case ImageSerializationTag::kCanvasColorSpaceTag:
              if (!ReadUint32Enum<SerializedColorSpace>(&canvas_color_space))
                return nullptr;
              break;
            case ImageSerializationTag::kImageDataStorageFormatTag:
              if (!ReadUint32Enum<SerializedImageDataStorageFormat>(
                      &image_data_storage_format))
                return nullptr;
              break;
            default:
              NOTREACHED();
          }
        } while (!is_done);
      }
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&byte_length) || !ReadRawBytes(byte_length, &pixels))
        return nullptr;
      SerializedColorParams color_params(
          canvas_color_space, SerializedPixelFormat::kRGBA8,
          SerializedOpacityMode::kNonOpaque, image_data_storage_format);
      ImageDataStorageFormat storage_format = color_params.GetStorageFormat();
      base::CheckedNumeric<uint32_t> computed_byte_length = width;
      computed_byte_length *= height;
      computed_byte_length *= 4;
      computed_byte_length *= ImageData::StorageFormatDataSize(storage_format);
      if (!computed_byte_length.IsValid() ||
          computed_byte_length.ValueOrDie() != byte_length)
        return nullptr;
      ImageData* image_data = ImageData::Create(
          IntSize(width, height), color_params.GetColorSpace(), storage_format);
      if (!image_data)
        return nullptr;
      DOMArrayBufferBase* pixel_buffer = image_data->BufferBase();
      DCHECK_EQ(pixel_buffer->ByteLength(), byte_length);
      memcpy(pixel_buffer->Data(), pixels, byte_length);
      return image_data;
    }
    case kDOMPointTag: {
      double x = 0, y = 0, z = 0, w = 1;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
          !ReadDouble(&w))
        return nullptr;
      return DOMPoint::Create(x, y, z, w);
    }
    case kDOMPointReadOnlyTag: {
      double x = 0, y = 0, z = 0, w = 1;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
          !ReadDouble(&w))
        return nullptr;
      return DOMPointReadOnly::Create(x, y, z, w);
    }
    case kDOMRectTag: {
      double x = 0, y = 0, width = 0, height = 0;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&width) ||
          !ReadDouble(&height))
        return nullptr;
      return DOMRect::Create(x, y, width, height);
    }
    case kDOMRectReadOnlyTag: {
      return ReadDOMRectReadOnly();
    }
    case kDOMQuadTag: {
      DOMPointInit pointInits[4];
      for (DOMPointInit& init : pointInits) {
        double x = 0, y = 0, z = 0, w = 0;
        if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
            !ReadDouble(&w))
          return nullptr;
        init.setX(x);
        init.setY(y);
        init.setZ(z);
        init.setW(w);
      }
      return DOMQuad::Create(pointInits[0], pointInits[1], pointInits[2],
                             pointInits[3]);
    }
    case kDOMMatrix2DTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values, base::size(values));
    }
    case kDOMMatrix2DReadOnlyTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(values,
                                                       base::size(values));
    }
    case kDOMMatrixTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values, base::size(values));
    }
    case kDOMMatrixReadOnlyTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(values,
                                                       base::size(values));
    }
    case kMessagePortTag: {
      uint32_t index = 0;
      if (!ReadUint32(&index) || !transferred_message_ports_ ||
          index >= transferred_message_ports_->size())
        return nullptr;
      return (*transferred_message_ports_)[index].Get();
    }
    case kMojoHandleTag: {
      uint32_t index = 0;
      if (!RuntimeEnabledFeatures::MojoJSEnabled() || !ReadUint32(&index) ||
          index >= serialized_script_value_->MojoHandles().size()) {
        return nullptr;
      }
      return MojoHandle::Create(
          std::move(serialized_script_value_->MojoHandles()[index]));
    }
    case kOffscreenCanvasTransferTag: {
      uint32_t width = 0, height = 0, canvas_id = 0, client_id = 0, sink_id = 0;
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&canvas_id) || !ReadUint32(&client_id) ||
          !ReadUint32(&sink_id))
        return nullptr;
      OffscreenCanvas* canvas = OffscreenCanvas::Create(width, height);
      canvas->SetPlaceholderCanvasId(canvas_id);
      canvas->SetFrameSinkId(client_id, sink_id);
      return canvas;
    }
    default:
      break;
  }
  return nullptr;
}

File* V8ScriptValueDeserializer::ReadFile() {
  if (Version() < 3)
    return nullptr;
  String path, name, relative_path, uuid, type;
  uint32_t has_snapshot = 0;
  uint64_t size = 0;
  double last_modified_ms = 0;
  if (!ReadUTF8String(&path) || (Version() >= 4 && !ReadUTF8String(&name)) ||
      (Version() >= 4 && !ReadUTF8String(&relative_path)) ||
      !ReadUTF8String(&uuid) || !ReadUTF8String(&type) ||
      (Version() >= 4 && !ReadUint32(&has_snapshot)))
    return nullptr;
  if (has_snapshot) {
    if (!ReadUint64(&size) || !ReadDouble(&last_modified_ms))
      return nullptr;
    if (Version() < 8)
      last_modified_ms *= kMsPerSecond;
  }
  uint32_t is_user_visible = 1;
  if (Version() >= 7 && !ReadUint32(&is_user_visible))
    return nullptr;
  const File::UserVisibility user_visibility =
      is_user_visible ? File::kIsUserVisible : File::kIsNotUserVisible;
  const uint64_t kSizeForDataHandle = static_cast<uint64_t>(-1);
  auto blob_handle = GetOrCreateBlobDataHandle(uuid, type, kSizeForDataHandle);
  if (!blob_handle)
    return nullptr;
  return File::CreateFromSerialization(
      path, name, relative_path, user_visibility, has_snapshot, size,
      last_modified_ms, std::move(blob_handle));
}

File* V8ScriptValueDeserializer::ReadFileIndex() {
  if (Version() < 6 || !blob_info_array_)
    return nullptr;
  uint32_t index;
  if (!ReadUint32(&index) || index >= blob_info_array_->size())
    return nullptr;
  const WebBlobInfo& info = (*blob_info_array_)[index];
  // FIXME: transition WebBlobInfo.lastModified to be milliseconds-based also.
  double last_modified_ms = info.LastModified() * kMsPerSecond;
  auto blob_handle = info.GetBlobHandle();
  if (!blob_handle) {
    blob_handle =
        GetOrCreateBlobDataHandle(info.Uuid(), info.GetType(), info.size());
  }
  if (!blob_handle)
    return nullptr;
  return File::CreateFromIndexedSerialization(info.FilePath(), info.FileName(),
                                              info.size(), last_modified_ms,
                                              blob_handle);
}

DOMRectReadOnly* V8ScriptValueDeserializer::ReadDOMRectReadOnly() {
  double x = 0, y = 0, width = 0, height = 0;
  if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&width) ||
      !ReadDouble(&height))
    return nullptr;
  return DOMRectReadOnly::Create(x, y, width, height);
}

scoped_refptr<BlobDataHandle>
V8ScriptValueDeserializer::GetOrCreateBlobDataHandle(const String& uuid,
                                                     const String& type,
                                                     uint64_t size) {
  // The containing ssv may have a BDH for this uuid if this ssv is just being
  // passed from main to worker thread (for example). We use those values when
  // creating the new blob instead of cons'ing up a new BDH.
  //
  // FIXME: Maybe we should require that it work that way where the ssv must
  // have a BDH for any blobs it comes across during deserialization. Would
  // require callers to explicitly populate the collection of BDH's for blobs to
  // work, which would encourage lifetimes to be considered when passing ssv's
  // around cross process. At present, we get 'lucky' in some cases because the
  // blob in the src process happens to still exist at the time the dest process
  // is deserializing.
  // For example in sharedWorker.postMessage(...).
  BlobDataHandleMap& handles = serialized_script_value_->BlobDataHandles();
  BlobDataHandleMap::const_iterator it = handles.find(uuid);
  if (it != handles.end())
    return it->value;
  // Creating a BlobDataHandle from an empty string will get this renderer
  // killed, so since we're parsing untrusted data (from possibly another
  // process/renderer) return null instead.
  if (uuid.IsEmpty())
    return nullptr;
  return BlobDataHandle::Create(uuid, type, size);
}

v8::MaybeLocal<v8::Object> V8ScriptValueDeserializer::ReadHostObject(
    v8::Isolate* isolate) {
  DCHECK_EQ(isolate, script_state_->GetIsolate());
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  ScriptWrappable* wrappable = nullptr;
  SerializationTag tag = kVersionTag;
  if (ReadTag(&tag))
    wrappable = ReadDOMObject(tag);
  if (!wrappable) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "Unable to deserialize cloned data.");
    return v8::MaybeLocal<v8::Object>();
  }
  v8::Local<v8::Object> creation_context =
      script_state_->GetContext()->Global();
  v8::Local<v8::Value> wrapper = ToV8(wrappable, creation_context, isolate);
  DCHECK(wrapper->IsObject());
  return wrapper.As<v8::Object>();
}

v8::MaybeLocal<v8::WasmCompiledModule>
V8ScriptValueDeserializer::GetWasmModuleFromId(v8::Isolate* isolate,
                                               uint32_t id) {
  if (id < serialized_script_value_->WasmModules().size()) {
    return v8::WasmCompiledModule::FromTransferrableModule(
        isolate, serialized_script_value_->WasmModules()[id]);
  }
  CHECK(serialized_script_value_->WasmModules().IsEmpty());
  return v8::MaybeLocal<v8::WasmCompiledModule>();
}

v8::MaybeLocal<v8::SharedArrayBuffer>
V8ScriptValueDeserializer::GetSharedArrayBufferFromId(v8::Isolate* isolate,
                                                      uint32_t id) {
  auto& shared_array_buffers_contents =
      serialized_script_value_->SharedArrayBuffersContents();
  if (id < shared_array_buffers_contents.size()) {
    WTF::ArrayBufferContents& contents = shared_array_buffers_contents.at(id);
    DOMSharedArrayBuffer* shared_array_buffer =
        DOMSharedArrayBuffer::Create(contents);
    v8::Local<v8::Object> creation_context =
        script_state_->GetContext()->Global();
    v8::Local<v8::Value> wrapper =
        ToV8(shared_array_buffer, creation_context, isolate);
    DCHECK(wrapper->IsSharedArrayBuffer());
    return v8::Local<v8::SharedArrayBuffer>::Cast(wrapper);
  }
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                    "Unable to deserialize SharedArrayBuffer.");
  // If the id does not map to a valid index, it is expected that the
  // SerializedScriptValue emptied its shared ArrayBufferContents when crossing
  // a process boundary.
  CHECK(shared_array_buffers_contents.IsEmpty());
  return v8::MaybeLocal<v8::SharedArrayBuffer>();
}
}  // namespace blink
