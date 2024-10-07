// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"

#include <limits>
#include <optional>

#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fenced_frame_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_handle.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_offscreen_canvas.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_transform_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
    if (shift < 32) [[likely]] {
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

  // These versions expect a trailer offset in the envelope.
  if (version >= TrailerReader::kMinWireFormatVersion) {
    static constexpr size_t kTrailerOffsetDataSize =
        1 + sizeof(uint64_t) + sizeof(uint32_t);
    DCHECK_LT(i, std::numeric_limits<size_t>::max() - kTrailerOffsetDataSize);
    i += kTrailerOffsetDataSize;
    if (i >= length)
      return 0;
  }

  // Otherwise, we did read the envelope. Hurray!
  *out_version = version;
  return i;
}

MessagePort* CreateEntangledPort(ScriptState* script_state,
                                 const MessagePortChannel& channel) {
  MessagePort* const port =
      MakeGarbageCollected<MessagePort>(*ExecutionContext::From(script_state));
  port->Entangle(channel);
  return port;
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
  // TODO(ricea): Make ExtendableMessageEvent store an
  // UnpackedSerializedScriptValue like MessageEvent does, and then this
  // special case won't be necessary.
  streams_ = std::move(serialized_script_value_->GetStreams());

  // There's nothing else to transfer if the deserializer was not given an
  // unpacked value.
  if (!unpacked_value_)
    return;

  // Transfer array buffers.
  const auto& array_buffers = unpacked_value_->ArrayBuffers();
  for (unsigned i = 0; i < array_buffers.size(); i++) {
    DOMArrayBufferBase* array_buffer = array_buffers.at(i);
    v8::Local<v8::Value> wrapper =
        ToV8Traits<DOMArrayBufferBase>::ToV8(script_state_, array_buffer);
    if (array_buffer->IsShared()) {
      // Crash if we are receiving a SharedArrayBuffer and this isn't allowed.
      auto* execution_context = ExecutionContext::From(script_state_);
      CHECK(execution_context->SharedArrayBufferTransferAllowed());

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

bool V8ScriptValueDeserializer::ReadUnguessableToken(
    base::UnguessableToken* token_out) {
  uint64_t high;
  uint64_t low;
  if (!ReadUint64(&high) || !ReadUint64(&low))
    return false;
  std::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return false;
  }
  *token_out = token.value();
  return true;
}

bool V8ScriptValueDeserializer::ReadUTF8String(String* string) {
  uint32_t utf8_length = 0;
  const void* utf8_data = nullptr;
  if (!ReadUint32(&utf8_length) || !ReadRawBytes(utf8_length, &utf8_data))
    return false;
  *string =
      String::FromUTF8(reinterpret_cast<const LChar*>(utf8_data), utf8_length);

  // Decoding must have failed; this encoding does not distinguish between null
  // and empty strings.
  return !string->IsNull();
}

ScriptWrappable* V8ScriptValueDeserializer::ReadDOMObject(
    SerializationTag tag,
    ExceptionState& exception_state) {
  if (!ExecutionContextExposesInterface(
          ExecutionContext::From(GetScriptState()), tag)) {
    return nullptr;
  }
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
      return MakeGarbageCollected<Blob>(std::move(blob_handle));
    }
    case kBlobIndexTag: {
      if (Version() < 6 || !blob_info_array_)
        return nullptr;
      uint32_t index = 0;
      if (!ReadUint32(&index) || index >= blob_info_array_->size())
        return nullptr;
      const WebBlobInfo& info = (*blob_info_array_)[index];
      auto blob_handle = info.GetBlobHandle();
      if (!blob_handle)
        return nullptr;
      return MakeGarbageCollected<Blob>(std::move(blob_handle));
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
      auto* file_list = MakeGarbageCollected<FileList>();
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
      auto* file_list = MakeGarbageCollected<FileList>();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = ReadFileIndex())
          file_list->Append(file);
        else
          return nullptr;
      }
      return file_list;
    }
    case kImageBitmapTag: {
      SerializedPredefinedColorSpace predefined_color_space =
          SerializedPredefinedColorSpace::kSRGB;
      Vector<double> sk_color_space;
      SerializedPixelFormat canvas_pixel_format =
          SerializedPixelFormat::kNative8_LegacyObsolete;
      SerializedOpacityMode canvas_opacity_mode =
          SerializedOpacityMode::kOpaque;
      SerializedImageOrientation image_orientation =
          SerializedImageOrientation::kTopLeft;
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
            case ImageSerializationTag::kPredefinedColorSpaceTag:
              if (!ReadUint32Enum<SerializedPredefinedColorSpace>(
                      &predefined_color_space)) {
                return nullptr;
              }
              break;
            case ImageSerializationTag::kParametricColorSpaceTag:
              sk_color_space.resize(kSerializedParametricColorSpaceLength);
              for (double& value : sk_color_space) {
                if (!ReadDouble(&value))
                  return nullptr;
              }
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
            case ImageSerializationTag::kImageOrientationTag:
              if (!ReadUint32Enum<SerializedImageOrientation>(
                      &image_orientation))
                return nullptr;
              break;
            case ImageSerializationTag::kImageDataStorageFormatTag:
              // Does not apply to ImageBitmap.
              return nullptr;
          }
        } while (!is_done);
      } else if (!ReadUint32(&origin_clean) || origin_clean > 1 ||
                 !ReadUint32(&is_premultiplied) || is_premultiplied > 1) {
        return nullptr;
      }
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&byte_length) || !ReadRawBytes(byte_length, &pixels))
        return nullptr;
      SerializedImageBitmapSettings settings(
          predefined_color_space, sk_color_space, canvas_pixel_format,
          canvas_opacity_mode, is_premultiplied, image_orientation);
      SkImageInfo info = settings.GetSkImageInfo(width, height);
      base::CheckedNumeric<uint32_t> computed_byte_length =
          info.computeMinByteSize();
      if (!computed_byte_length.IsValid() ||
          computed_byte_length.ValueOrDie() != byte_length)
        return nullptr;
      if (!origin_clean) {
        // Non-origin-clean ImageBitmap serialization/deserialization have
        // been deprecated.
        return nullptr;
      }
      SkPixmap pixmap(info, pixels, info.minRowBytes());
      return MakeGarbageCollected<ImageBitmap>(pixmap, origin_clean,
                                               settings.GetImageOrientation());
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
      SerializedPredefinedColorSpace predefined_color_space =
          SerializedPredefinedColorSpace::kSRGB;
      SerializedImageDataStorageFormat image_data_storage_format =
          SerializedImageDataStorageFormat::kUint8Clamped;
      uint32_t width = 0, height = 0;
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
            case ImageSerializationTag::kPredefinedColorSpaceTag:
              if (!ReadUint32Enum<SerializedPredefinedColorSpace>(
                      &predefined_color_space))
                return nullptr;
              break;
            case ImageSerializationTag::kImageDataStorageFormatTag:
              if (!ReadUint32Enum<SerializedImageDataStorageFormat>(
                      &image_data_storage_format))
                return nullptr;
              break;
            case ImageSerializationTag::kCanvasPixelFormatTag:
            case ImageSerializationTag::kOriginCleanTag:
            case ImageSerializationTag::kIsPremultipliedTag:
            case ImageSerializationTag::kCanvasOpacityModeTag:
            case ImageSerializationTag::kParametricColorSpaceTag:
            case ImageSerializationTag::kImageOrientationTag:
              // Does not apply to ImageData.
              return nullptr;
          }
        } while (!is_done);
      }

      uint64_t byte_length_64 = 0;
      size_t byte_length = 0;
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint64(&byte_length_64) ||
          !base::MakeCheckedNum(byte_length_64).AssignIfValid(&byte_length) ||
          !ReadRawBytes(byte_length, &pixels)) {
        return nullptr;
      }

      SerializedImageDataSettings settings(predefined_color_space,
                                           image_data_storage_format);
      ImageData* image_data = ImageData::ValidateAndCreate(
          width, height, std::nullopt, settings.GetImageDataSettings(),
          ImageData::ValidateAndCreateParams(), exception_state);
      if (!image_data)
        return nullptr;
      SkPixmap image_data_pixmap = image_data->GetSkPixmap();
      if (image_data_pixmap.computeByteSize() != byte_length)
        return nullptr;
      memcpy(image_data_pixmap.writable_addr(), pixels, byte_length);
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
      DOMPointInit* point_inits[4];
      for (int i = 0; i < 4; ++i) {
        auto* init = DOMPointInit::Create();
        double x = 0, y = 0, z = 0, w = 0;
        if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
            !ReadDouble(&w))
          return nullptr;
        init->setX(x);
        init->setY(y);
        init->setZ(z);
        init->setW(w);
        point_inits[i] = init;
      }
      return DOMQuad::Create(point_inits[0], point_inits[1], point_inits[2],
                             point_inits[3]);
    }
    case kDOMMatrix2DTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values, std::size(values));
    }
    case kDOMMatrix2DReadOnlyTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(values,
                                                       std::size(values));
    }
    case kDOMMatrixTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values, std::size(values));
    }
    case kDOMMatrixReadOnlyTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(values,
                                                       std::size(values));
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
      return MakeGarbageCollected<MojoHandle>(
          std::move(serialized_script_value_->MojoHandles()[index]));
    }
    case kOffscreenCanvasTransferTag: {
      uint32_t width = 0, height = 0, canvas_id = 0, client_id = 0, sink_id = 0,
               filter_quality = 0;
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&canvas_id) || !ReadUint32(&client_id) ||
          !ReadUint32(&sink_id) || !ReadUint32(&filter_quality))
        return nullptr;
      OffscreenCanvas* canvas =
          OffscreenCanvas::Create(GetScriptState(), width, height);
      canvas->SetPlaceholderCanvasId(canvas_id);
      canvas->SetFrameSinkId(client_id, sink_id);
      if (filter_quality == 0)
        canvas->SetFilterQuality(cc::PaintFlags::FilterQuality::kNone);
      else
        canvas->SetFilterQuality(cc::PaintFlags::FilterQuality::kLow);
      return canvas;
    }
    case kReadableStreamTransferTag: {
      uint32_t index = 0;
      if (!ReadUint32(&index) || index >= streams_.size()) {
        return nullptr;
      }
      return ReadableStream::Deserialize(
          script_state_,
          CreateEntangledPort(GetScriptState(), streams_[index].channel),
          std::move(streams_[index].readable_optimizer), exception_state);
    }
    case kWritableStreamTransferTag: {
      uint32_t index = 0;
      if (!ReadUint32(&index) || index >= streams_.size()) {
        return nullptr;
      }
      return WritableStream::Deserialize(
          script_state_,
          CreateEntangledPort(GetScriptState(), streams_[index].channel),
          std::move(streams_[index].writable_optimizer), exception_state);
    }
    case kTransformStreamTransferTag: {
      uint32_t index = 0;
      if (!ReadUint32(&index) ||
          index == std::numeric_limits<decltype(index)>::max() ||
          index + 1 >= streams_.size()) {
        return nullptr;
      }
      MessagePort* const port_for_readable =
          CreateEntangledPort(GetScriptState(), streams_[index].channel);
      MessagePort* const port_for_writable =
          CreateEntangledPort(GetScriptState(), streams_[index + 1].channel);

      // https://streams.spec.whatwg.org/#ts-transfer
      // 1. Let readableRecord be !
      //    StructuredDeserializeWithTransfer(dataHolder.[[readable]], the
      //    current Realm).
      ReadableStream* readable =
          ReadableStream::Deserialize(script_state_, port_for_readable,
                                      /*optimizer=*/nullptr, exception_state);
      if (!readable)
        return nullptr;

      // 2. Let writableRecord be !
      //    StructuredDeserializeWithTransfer(dataHolder.[[writable]], the
      //    current Realm).
      WritableStream* writable =
          WritableStream::Deserialize(script_state_, port_for_writable,
                                      /*optimizer=*/nullptr, exception_state);
      if (!writable)
        return nullptr;

      // 3. Set value.[[readable]] to readableRecord.[[Deserialized]].
      // 4. Set value.[[writable]] to writableRecord.[[Deserialized]].
      // 5. Set value.[[backpressure]], value.[[backpressureChangePromise]], and
      //    value.[[controller]] to undefined.
      return MakeGarbageCollected<TransformStream>(readable, writable);
    }
    case kDOMExceptionTag: {
      // See the serialization side for |stack_unused|.
      String name, message, stack_unused;
      if (!ReadUTF8String(&name) || !ReadUTF8String(&message) ||
          !ReadUTF8String(&stack_unused)) {
        return nullptr;
      }
      // DOMException::Create takes its arguments in the opposite order.
      return DOMException::Create(message, name);
    }
    case kFencedFrameConfigTag: {
      String url_string, shared_storage_context, urn_uuid_string;
      uint32_t has_shared_storage_context, has_container_size, container_width,
          container_height, has_content_size, content_width, content_height,
          freeze_initial_size;
      KURL url;
      std::optional<KURL> urn_uuid;
      FencedFrameConfig::AttributeVisibility url_visibility;
      std::optional<gfx::Size> container_size, content_size;

      if (!ReadUTF8String(&url_string) ||
          !ReadUint32Enum<FencedFrameConfig::AttributeVisibility>(
              &url_visibility) ||
          !ReadUint32(&freeze_initial_size) ||
          !ReadUTF8String(&urn_uuid_string)) {
        return nullptr;
      }

      // `ReadUTF8String` does not distinguish between null and empty strings.
      // Adding the `has_shared_storage_context` bit allows us to get this
      // functionality back, which is needed for Shared Storage.
      if (!ReadUint32(&has_shared_storage_context)) {
        return nullptr;
      }
      if (has_shared_storage_context &&
          !ReadUTF8String(&shared_storage_context)) {
        return nullptr;
      }

      if (!ReadUint32(&has_container_size)) {
        return nullptr;
      }
      if (has_container_size) {
        if (!ReadUint32(&container_width) || !ReadUint32(&container_height)) {
          return nullptr;
        }
        container_size = gfx::Size(container_width, container_height);
      }

      if (!ReadUint32(&has_content_size)) {
        return nullptr;
      }
      if (has_content_size) {
        if (!ReadUint32(&content_width) || !ReadUint32(&content_height)) {
          return nullptr;
        }
        content_size = gfx::Size(content_width, content_height);
      }

      // Validate the URL and URN values.
      url = KURL(url_string);
      if (!url.IsEmpty() && !url.IsValid()) {
        return nullptr;
      }
      if (blink::IsValidUrnUuidURL(GURL(urn_uuid_string.Utf8()))) {
        urn_uuid = KURL(urn_uuid_string);
      } else if (!urn_uuid_string.empty()) {
        return nullptr;
      }

      return FencedFrameConfig::Create(url, shared_storage_context, urn_uuid,
                                       container_size, content_size,
                                       url_visibility, freeze_initial_size);
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
  std::optional<base::Time> last_modified;
  if (has_snapshot && std::isfinite(last_modified_ms)) {
    last_modified =
        base::Time::FromMillisecondsSinceUnixEpoch(last_modified_ms);
  }
  return File::CreateFromSerialization(path, name, relative_path,
                                       user_visibility, has_snapshot, size,
                                       last_modified, std::move(blob_handle));
}

File* V8ScriptValueDeserializer::ReadFileIndex() {
  if (Version() < 6 || !blob_info_array_)
    return nullptr;
  uint32_t index;
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
  return File::CreateFromIndexedSerialization(info.FileName(), info.size(),
                                              info.LastModified(), blob_handle);
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
  if (uuid.empty())
    return nullptr;
  return BlobDataHandle::Create(uuid, type, size);
}

v8::MaybeLocal<v8::Object> V8ScriptValueDeserializer::ReadHostObject(
    v8::Isolate* isolate) {
  DCHECK_EQ(isolate, script_state_->GetIsolate());
  ExceptionState exception_state(isolate, v8::ExceptionContext::kUnknown,
                                 nullptr, nullptr);
  ScriptWrappable* wrappable = nullptr;
  SerializationTag tag = kVersionTag;
  if (ReadTag(&tag)) {
    wrappable = ReadDOMObject(tag, exception_state);
    if (exception_state.HadException())
      return v8::MaybeLocal<v8::Object>();
  }
  if (!wrappable) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "Unable to deserialize cloned data.");
    return v8::MaybeLocal<v8::Object>();
  }
  v8::Local<v8::Value> wrapper =
      ToV8Traits<ScriptWrappable>::ToV8(script_state_, wrappable);
  DCHECK(wrapper->IsObject());
  return wrapper.As<v8::Object>();
}

v8::MaybeLocal<v8::WasmModuleObject>
V8ScriptValueDeserializer::GetWasmModuleFromId(v8::Isolate* isolate,
                                               uint32_t id) {
  if (id < serialized_script_value_->WasmModules().size()) {
    return v8::WasmModuleObject::FromCompiledModule(
        isolate, serialized_script_value_->WasmModules()[id]);
  }
  CHECK(serialized_script_value_->WasmModules().empty());
  return v8::MaybeLocal<v8::WasmModuleObject>();
}

v8::MaybeLocal<v8::SharedArrayBuffer>
V8ScriptValueDeserializer::GetSharedArrayBufferFromId(v8::Isolate* isolate,
                                                      uint32_t id) {
  auto& shared_array_buffers_contents =
      serialized_script_value_->SharedArrayBuffersContents();
  if (id < shared_array_buffers_contents.size()) {
    ArrayBufferContents& contents = shared_array_buffers_contents.at(id);
    DOMSharedArrayBuffer* shared_array_buffer =
        DOMSharedArrayBuffer::Create(contents);
    v8::Local<v8::Value> wrapper = ToV8Traits<DOMSharedArrayBuffer>::ToV8(
        script_state_, shared_array_buffer);
    DCHECK(wrapper->IsSharedArrayBuffer());
    return v8::Local<v8::SharedArrayBuffer>::Cast(wrapper);
  }
  ExceptionState exception_state(isolate, v8::ExceptionContext::kUnknown,
                                 nullptr, nullptr);
  exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                    "Unable to deserialize SharedArrayBuffer.");
  // If the id does not map to a valid index, it is expected that the
  // SerializedScriptValue emptied its shared ArrayBufferContents when crossing
  // a process boundary.
  CHECK(shared_array_buffers_contents.empty());
  return v8::MaybeLocal<v8::SharedArrayBuffer>();
}

const v8::SharedValueConveyor*
V8ScriptValueDeserializer::GetSharedValueConveyor(v8::Isolate* isolate) {
  if (auto* conveyor =
          serialized_script_value_->MaybeGetSharedValueConveyor()) {
    return conveyor;
  }
  ExceptionState exception_state(isolate, v8::ExceptionContext::kUnknown,
                                 nullptr, nullptr);
  exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                    "Unable to deserialize shared JS value.");
  return nullptr;
}

// static
bool V8ScriptValueDeserializer::ExecutionContextExposesInterface(
    ExecutionContext* execution_context,
    SerializationTag interface_tag) {
  // If you're updating this, consider whether you should also update
  // V8ScriptValueSerializer to call TrailerWriter::RequireExposedInterface
  // (generally via WriteAndRequireInterfaceTag). Any interface which might
  // potentially not be exposed on all realms, even if not currently (i.e., most
  // or all) should probably be listed here.
  switch (interface_tag) {
    case kBlobTag:
    case kBlobIndexTag:
      return V8Blob::IsExposed(execution_context);
    case kFileTag:
    case kFileIndexTag:
      return V8File::IsExposed(execution_context);
    case kFileListTag:
    case kFileListIndexTag: {
      const bool is_exposed = V8FileList::IsExposed(execution_context);
      if (is_exposed)
        DCHECK(V8File::IsExposed(execution_context));
      return is_exposed;
    }
    case kImageBitmapTag:
    case kImageBitmapTransferTag:
      return V8ImageBitmap::IsExposed(execution_context);
    case kImageDataTag:
      return V8ImageData::IsExposed(execution_context);
    case kDOMPointTag:
      return V8DOMPoint::IsExposed(execution_context);
    case kDOMPointReadOnlyTag:
      return V8DOMPointReadOnly::IsExposed(execution_context);
    case kDOMRectTag:
      return V8DOMRect::IsExposed(execution_context);
    case kDOMRectReadOnlyTag:
      return V8DOMRectReadOnly::IsExposed(execution_context);
    case kDOMQuadTag:
      return V8DOMQuad::IsExposed(execution_context);
    case kDOMMatrix2DTag:
    case kDOMMatrixTag:
      return V8DOMMatrix::IsExposed(execution_context);
    case kDOMMatrix2DReadOnlyTag:
    case kDOMMatrixReadOnlyTag:
      return V8DOMMatrixReadOnly::IsExposed(execution_context);
    case kMessagePortTag:
      return V8MessagePort::IsExposed(execution_context);
    case kMojoHandleTag:
      // This would ideally be V8MojoHandle::IsExposed, but WebUSB tests
      // currently rely on being able to send handles to frames and workers
      // which don't otherwise have MojoJS exposed.
      return (execution_context->IsWindow() ||
              execution_context->IsWorkerGlobalScope()) &&
             RuntimeEnabledFeatures::MojoJSEnabled();
    case kOffscreenCanvasTransferTag:
      return V8OffscreenCanvas::IsExposed(execution_context);
    case kReadableStreamTransferTag:
      return V8ReadableStream::IsExposed(execution_context);
    case kWritableStreamTransferTag:
      return V8WritableStream::IsExposed(execution_context);
    case kTransformStreamTransferTag: {
      const bool is_exposed = V8TransformStream::IsExposed(execution_context);
      if (is_exposed) {
        DCHECK(V8ReadableStream::IsExposed(execution_context));
        DCHECK(V8WritableStream::IsExposed(execution_context));
      }
      return is_exposed;
    }
    case kDOMExceptionTag:
      return V8DOMException::IsExposed(execution_context);
    case kFencedFrameConfigTag:
      return V8FencedFrameConfig::IsExposed(execution_context);
    default:
      return false;
  }
}

}  // namespace blink
