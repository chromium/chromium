/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/canvas/image_data.h"

#include "base/sys_byteorder.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_clamped_array.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "v8/include/v8.h"

namespace blink {

ImageData* ImageData::ValidateAndCreate(
    unsigned width,
    base::Optional<unsigned> height,
    base::Optional<NotShared<DOMArrayBufferView>> data,
    const ImageDataSettings* input_settings,
    ExceptionState& exception_state,
    uint32_t flags) {
  IntSize size;
  if ((flags & RequireCanvasColorManagement &&
       !RuntimeEnabledFeatures::CanvasColorManagementEnabled())) {
    exception_state.ThrowTypeError("Overload resolution failed.");
    return nullptr;
  }

  if (!width) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The source width is zero or not a number.");
    return nullptr;
  }
  size.SetWidth(width);
  if (height) {
    if (!*height) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "The source height is zero or not a number.");
      return nullptr;
    }
    size.SetHeight(*height);
  }

  // Populate the ImageDataSettings to use based on |input_settings|.
  ImageDataSettings* settings = ImageDataSettings::Create();
  if (input_settings) {
    settings->setColorSpace(input_settings->colorSpace());
    settings->setStorageFormat(input_settings->storageFormat());
  }

  // Ensure the size does not overflow.
  unsigned size_in_elements = 0;
  {
    // Please note that the number "4" in the means number of channels required
    // to describe a pixel, namely, red, green, blue and alpha.
    base::CheckedNumeric<unsigned> size_in_elements_checked = 4;
    size_in_elements_checked *= size.Width();
    size_in_elements_checked *= size.Height();
    if (!(flags & ValidateAndCreateFlags::Context2DErrorMode)) {
      if (!size_in_elements_checked.IsValid()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The requested image size exceeds the supported range.");
        return nullptr;
      }
    }
    if (!size_in_elements_checked.IsValid() ||
        size_in_elements_checked.ValueOrDie() > v8::TypedArray::kMaxLength) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation.");
      return nullptr;
    }
    size_in_elements = size_in_elements_checked.ValueOrDie();
  }

  // If |data| is provided, ensure it is a reasonable format, and that it can
  // work with |size|. Update |settings| to reflect |data|'s format.
  if (data) {
    DCHECK(data);
    switch ((*data)->GetType()) {
      case DOMArrayBufferView::ViewType::kTypeUint8Clamped:
        settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
        break;
      case DOMArrayBufferView::ViewType::kTypeUint16:
        settings->setStorageFormat(kUint16ArrayStorageFormatName);
        break;
      case DOMArrayBufferView::ViewType::kTypeFloat32:
        settings->setStorageFormat(kFloat32ArrayStorageFormatName);
        break;
      default:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "The input data type is not supported.");
        return nullptr;
    }
    static_assert(
        std::numeric_limits<unsigned>::max() >=
            std::numeric_limits<uint32_t>::max(),
        "We use UINT32_MAX as the upper bound of the input size and expect "
        "that the result fits into an `unsigned`.");

    unsigned data_length_in_bytes = 0;
    if (!base::CheckedNumeric<uint32_t>((*data)->byteLength())
             .AssignIfValid(&data_length_in_bytes)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "The input data is too large. The maximum size is 4294967295.");
      return nullptr;
    }
    if (!data_length_in_bytes) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The input data has zero elements.");
      return nullptr;
    }

    const unsigned data_length_in_elements =
        data_length_in_bytes / (*data)->TypeSize();
    if (data_length_in_elements % 4) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The input data length is not a multiple of 4.");
      return nullptr;
    }

    const unsigned data_length_in_pixels = data_length_in_elements / 4;
    if (data_length_in_pixels % width) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "The input data length is not a multiple of (4 * width).");
      return nullptr;
    }

    const unsigned expected_height = data_length_in_pixels / width;
    if (height) {
      if (*height != expected_height) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The input data length is not equal to (4 * width * height).");
        return nullptr;
      }
    } else {
      size.SetHeight(expected_height);
    }
  }

  NotShared<DOMArrayBufferView> allocated_data;
  if (!data) {
    ImageDataStorageFormat storage_format =
        GetImageDataStorageFormat(settings->storageFormat());
    allocated_data = AllocateAndValidateDataArray(
        size_in_elements, storage_format, &exception_state);
    if (!allocated_data)
      return nullptr;
  }

  return MakeGarbageCollected<ImageData>(size, data ? *data : allocated_data,
                                         settings);
}

NotShared<DOMArrayBufferView> ImageData::AllocateAndValidateDataArray(
    const unsigned& length,
    ImageDataStorageFormat storage_format,
    ExceptionState* exception_state) {
  if (!length)
    return NotShared<DOMArrayBufferView>();

  NotShared<DOMArrayBufferView> data_array;
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      data_array = NotShared<DOMArrayBufferView>(
          DOMUint8ClampedArray::CreateOrNull(length));
      break;
    case kUint16ArrayStorageFormat:
      data_array =
          NotShared<DOMArrayBufferView>(DOMUint16Array::CreateOrNull(length));
      break;
    case kFloat32ArrayStorageFormat:
      data_array =
          NotShared<DOMArrayBufferView>(DOMFloat32Array::CreateOrNull(length));
      break;
    default:
      NOTREACHED();
  }

  size_t expected_size;
  if (!data_array || (!base::CheckMul(length, data_array->TypeSize())
                           .AssignIfValid(&expected_size) &&
                      expected_size != data_array->byteLength())) {
    if (exception_state)
      exception_state->ThrowRangeError("Out of memory at ImageData creation");
    return NotShared<DOMArrayBufferView>();
  }

  return data_array;
}

// This function accepts size (0, 0) and always returns the ImageData in
// "srgb" color space and "uint8" storage format.
ImageData* ImageData::CreateForTest(const IntSize& size) {
  base::CheckedNumeric<unsigned> data_size =
      StorageFormatBytesPerPixel(kUint8ClampedArrayStorageFormat);
  data_size *= size.Width();
  data_size *= size.Height();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return nullptr;

  NotShared<DOMUint8ClampedArray> byte_array(
      DOMUint8ClampedArray::CreateOrNull(data_size.ValueOrDie()));
  if (!byte_array)
    return nullptr;

  return MakeGarbageCollected<ImageData>(size, byte_array);
}

// This function is called from unit tests, and all the parameters are supposed
// to be validated on the call site.
ImageData* ImageData::CreateForTest(const IntSize& size,
                                    NotShared<DOMArrayBufferView> buffer_view,
                                    const ImageDataSettings* settings) {
  return MakeGarbageCollected<ImageData>(size, buffer_view, settings);
}

ScriptPromise ImageData::CreateImageBitmap(ScriptState* script_state,
                                           base::Optional<IntRect> crop_rect,
                                           const ImageBitmapOptions* options,
                                           ExceptionState& exception_state) {
  if (IsBufferBaseDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The source data has been detached.");
    return ScriptPromise();
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      exception_state);
}

String ImageData::CanvasColorSpaceName(CanvasColorSpace color_space) {
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      return kSRGBCanvasColorSpaceName;
    case CanvasColorSpace::kRec2020:
      return kRec2020CanvasColorSpaceName;
    case CanvasColorSpace::kP3:
      return kP3CanvasColorSpaceName;
    default:
      NOTREACHED();
  }
  return kSRGBCanvasColorSpaceName;
}

ImageDataStorageFormat ImageData::GetImageDataStorageFormat(
    const String& storage_format_name) {
  if (storage_format_name == kUint8ClampedArrayStorageFormatName)
    return kUint8ClampedArrayStorageFormat;
  if (storage_format_name == kUint16ArrayStorageFormatName)
    return kUint16ArrayStorageFormat;
  if (storage_format_name == kFloat32ArrayStorageFormatName)
    return kFloat32ArrayStorageFormat;
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

CanvasColorSpace ImageData::GetCanvasColorSpace() const {
  if (!RuntimeEnabledFeatures::CanvasColorManagementEnabled())
    return CanvasColorSpace::kSRGB;
  return CanvasColorSpaceFromName(settings_->colorSpace());
}

ImageDataStorageFormat ImageData::GetImageDataStorageFormat() const {
  if (data_u16_)
    return kUint16ArrayStorageFormat;
  if (data_f32_)
    return kFloat32ArrayStorageFormat;
  return kUint8ClampedArrayStorageFormat;
}

unsigned ImageData::StorageFormatBytesPerPixel(
    const String& storage_format_name) {
  if (storage_format_name == kUint8ClampedArrayStorageFormatName)
    return 4;
  if (storage_format_name == kUint16ArrayStorageFormatName)
    return 8;
  if (storage_format_name == kFloat32ArrayStorageFormatName)
    return 16;
  NOTREACHED();
  return 1;
}

unsigned ImageData::StorageFormatBytesPerPixel(
    ImageDataStorageFormat storage_format) {
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      return 4;
    case kUint16ArrayStorageFormat:
      return 8;
    case kFloat32ArrayStorageFormat:
      return 16;
  }
  NOTREACHED();
  return 1;
}

bool ImageData::IsBufferBaseDetached() const {
  if (data_.IsUint8ClampedArray())
    return data_.GetAsUint8ClampedArray()->BufferBase()->IsDetached();
  if (data_.IsUint16Array())
    return data_.GetAsUint16Array()->BufferBase()->IsDetached();
  if (data_.IsFloat32Array())
    return data_.GetAsFloat32Array()->BufferBase()->IsDetached();
  return false;
}

SkPixmap ImageData::GetSkPixmap() const {
  CHECK(!IsBufferBaseDetached());
  SkColorType color_type = kRGBA_8888_SkColorType;
  const void* data = nullptr;
  if (data_.IsUint8ClampedArray()) {
    color_type = kRGBA_8888_SkColorType;
    data = data_.GetAsUint8ClampedArray()->Data();
  } else if (data_.IsUint16Array()) {
    color_type = kR16G16B16A16_unorm_SkColorType;
    data = data_.GetAsUint16Array()->Data();
  } else if (data_.IsFloat32Array()) {
    color_type = kRGBA_F32_SkColorType;
    data = data_.GetAsFloat32Array()->Data();
  }
  SkImageInfo info =
      SkImageInfo::Make(width(), height(), color_type, kUnpremul_SkAlphaType,
                        CanvasColorSpaceToSkColorSpace(GetCanvasColorSpace()));
  return SkPixmap(info, data, info.minRowBytes());
}

void ImageData::Trace(Visitor* visitor) const {
  visitor->Trace(settings_);
  visitor->Trace(data_);
  visitor->Trace(data_u8_);
  visitor->Trace(data_u16_);
  visitor->Trace(data_f32_);
  ScriptWrappable::Trace(visitor);
}

v8::Local<v8::Object> ImageData::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  wrapper = ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type_info,
                                                  wrapper);

  if (data_.IsUint8ClampedArray()) {
    // Create a V8 object with |data_| and set the "data" property
    // of the ImageData object to the created v8 object, eliminating the
    // C++ callback when accessing the "data" property.
    //
    // This is a perf hack breaking the web interop.

    v8::Local<v8::Value> v8_data;
    ScriptState* script_state = ScriptState::From(wrapper->CreationContext());
    if (!ToV8Traits<IDLUnionNotINT<ImageDataArray>>::ToV8(script_state, data_)
             .ToLocal(&v8_data)) {
      return wrapper;
    }
    bool defined_property;
    if (!wrapper
             ->DefineOwnProperty(isolate->GetCurrentContext(),
                                 V8AtomicString(isolate, "data"), v8_data,
                                 v8::ReadOnly)
             .To(&defined_property)) {
      return wrapper;
    }
  }

  return wrapper;
}

ImageData::ImageData(const IntSize& size,
                     NotShared<DOMArrayBufferView> data,
                     const ImageDataSettings* settings)
    : size_(size), settings_(ImageDataSettings::Create()) {
  DCHECK_GE(size.Width(), 0);
  DCHECK_GE(size.Height(), 0);
  DCHECK(data);

  data_u8_.Clear();
  data_u16_.Clear();
  data_f32_.Clear();

  if (settings) {
    settings_->setColorSpace(settings->colorSpace());
    settings_->setStorageFormat(settings->storageFormat());
  }

  ImageDataStorageFormat storage_format =
      GetImageDataStorageFormat(settings_->storageFormat());
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      DCHECK_EQ(data->GetType(),
                DOMArrayBufferView::ViewType::kTypeUint8Clamped);
      data_u8_ = data;
      DCHECK(data_u8_);
      data_.SetUint8ClampedArray(data_u8_);
      SECURITY_CHECK(
          (base::CheckedNumeric<size_t>(size.Width()) * size.Height() * 4)
              .ValueOrDie() <= data_.GetAsUint8ClampedArray()->length());
      break;

    case kUint16ArrayStorageFormat:
      DCHECK_EQ(data->GetType(), DOMArrayBufferView::ViewType::kTypeUint16);
      data_u16_ = data;
      DCHECK(data_u16_);
      data_.SetUint16Array(data_u16_);
      SECURITY_CHECK(
          (base::CheckedNumeric<size_t>(size.Width()) * size.Height() * 4)
              .ValueOrDie() <= data_.GetAsUint16Array()->length());
      break;

    case kFloat32ArrayStorageFormat:
      DCHECK_EQ(data->GetType(), DOMArrayBufferView::ViewType::kTypeFloat32);
      data_f32_ = data;
      DCHECK(data_f32_);
      data_.SetFloat32Array(data_f32_);
      SECURITY_CHECK(
          (base::CheckedNumeric<size_t>(size.Width()) * size.Height() * 4)
              .ValueOrDie() <= data_.GetAsFloat32Array()->length());
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace blink
