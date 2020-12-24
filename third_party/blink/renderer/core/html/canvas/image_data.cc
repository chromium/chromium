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
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_clamped_array.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "v8/include/v8.h"

namespace blink {

// Please note that all the number "4" in the file means number of channels
// required to describe a pixel, namely, red, green, blue and alpha.
namespace {

ImageData* RaiseDOMExceptionAndReturnNull(ExceptionState* exception_state,
                                          DOMExceptionCode exception_code,
                                          const char* message) {
  if (exception_state)
    exception_state->ThrowDOMException(exception_code, message);
  return nullptr;
}

}  // namespace

ImageData* ImageData::ValidateAndCreate(const IntSize* input_size,
                                        const unsigned* width,
                                        const unsigned* height,
                                        NotShared<DOMArrayBufferView>* data,
                                        const ImageDataSettings* settings,
                                        ExceptionState* exception_state) {
  IntSize size;
  if (width) {
    DCHECK(!input_size);
    if (!*width) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The source width is zero or not a number.");
    }
    size.SetWidth(*width);
  }
  if (height) {
    DCHECK(width);
    if (!*height) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The source height is zero or not a number.");
    }
    size.SetHeight(*height);
  }

  // TODO(https://crbug.com/1160105): An |input_size| of 0x0 is accepted, but
  // |width| of 0 or |height| of 0 is not. Is this intentional?
  if (input_size)
    size = *input_size;

  // Ensure the size does not overflow.
  unsigned size_in_elements = 0;
  {
    base::CheckedNumeric<unsigned> size_in_elements_checked = 4;
    size_in_elements_checked *= size.Width();
    size_in_elements_checked *= size.Height();
    if (!size_in_elements_checked.IsValid()) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The requested image size exceeds the supported range.");
    }
    if (size_in_elements_checked.ValueOrDie() > v8::TypedArray::kMaxLength) {
      if (exception_state) {
        exception_state->ThrowRangeError(
            "Out of memory at ImageData creation.");
      }
      return nullptr;
    }
    size_in_elements = size_in_elements_checked.ValueOrDie();
  }

  // If |data| is provided, ensure it is a reasonable format, and that it can
  // work with |size|.
  if (data) {
    DCHECK(data);
    if ((*data)->GetType() != DOMArrayBufferView::ViewType::kTypeUint8Clamped &&
        (*data)->GetType() != DOMArrayBufferView::ViewType::kTypeUint16 &&
        (*data)->GetType() != DOMArrayBufferView::ViewType::kTypeFloat32) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The input data type is not supported.");
    }

    static_assert(
        std::numeric_limits<unsigned>::max() >=
            std::numeric_limits<uint32_t>::max(),
        "We use UINT32_MAX as the upper bound of the input size and expect "
        "that the result fits into an `unsigned`.");

    unsigned data_length_in_bytes = 0;
    if (!base::CheckedNumeric<uint32_t>((*data)->byteLength())
             .AssignIfValid(&data_length_in_bytes)) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The input data is too large. The maximum size is 4294967295.");
    }
    if (!data_length_in_bytes) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "The input data has zero elements.");
    }

    const unsigned data_length_in_elements =
        data_length_in_bytes / (*data)->TypeSize();
    if (data_length_in_elements % 4) {
      return RaiseDOMExceptionAndReturnNull(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "The input data length is not a multiple of 4.");
    }

    const unsigned data_length_in_pixels = data_length_in_elements / 4;
    // TODO(https://crbug.com/1160105): This code historically does not ensure
    // that |size| satisfy the same requirements when specified by |input_size|
    // as compared when when it is specified by |width| and |height|.
    if (width) {
      if (data_length_in_pixels % *width) {
        return RaiseDOMExceptionAndReturnNull(
            exception_state, DOMExceptionCode::kIndexSizeError,
            "The input data length is not a multiple of (4 * width).");
      }

      unsigned expected_height = data_length_in_pixels / *width;
      if (height) {
        if (*height != expected_height) {
          return RaiseDOMExceptionAndReturnNull(
              exception_state, DOMExceptionCode::kIndexSizeError,
              "The input data length is not equal to (4 * width * height).");
        }
      } else {
        size.SetHeight(expected_height);
      }
    }
    // As referenced above, this is is the only check that has been made when
    // size is specified by |input_size|.
    if (input_size) {
      if (size_in_elements > data_length_in_elements)
        return nullptr;
    }
  }

  NotShared<DOMArrayBufferView> allocated_data;
  if (!data) {
    ImageDataStorageFormat storage_format =
        settings ? GetImageDataStorageFormat(settings->storageFormat())
                 : kUint8ClampedArrayStorageFormat;
    allocated_data = AllocateAndValidateDataArray(
        size_in_elements, storage_format, exception_state);
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

ImageData* ImageData::Create(const IntSize& size,
                             const ImageDataSettings* settings) {
  return ValidateAndCreate(&size, nullptr, nullptr, nullptr, settings, nullptr);
}

ImageData* ImageData::Create(const IntSize& size,
                             CanvasColorSpace color_space,
                             ImageDataStorageFormat storage_format) {
  ImageDataSettings* settings = ImageDataSettings::Create();
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      settings->setColorSpace(kSRGBCanvasColorSpaceName);
      break;
    case CanvasColorSpace::kRec2020:
      settings->setColorSpace(kRec2020CanvasColorSpaceName);
      break;
    case CanvasColorSpace::kP3:
      settings->setColorSpace(kP3CanvasColorSpaceName);
      break;
  }

  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
      break;
    case kUint16ArrayStorageFormat:
      settings->setStorageFormat(kUint16ArrayStorageFormatName);
      break;
    case kFloat32ArrayStorageFormat:
      settings->setStorageFormat(kFloat32ArrayStorageFormatName);
      break;
  }

  return ImageData::Create(size, settings);
}

ImageData* ImageData::Create(const IntSize& size,
                             NotShared<DOMArrayBufferView> data_array,
                             const ImageDataSettings* settings) {
  NotShared<DOMArrayBufferView> buffer_view = data_array;
  return ValidateAndCreate(&size, nullptr, nullptr, &buffer_view, settings,
                           nullptr);
}

ImageData* ImageData::Create(unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  return ValidateAndCreate(nullptr, &width, &height, nullptr, nullptr,
                           &exception_state);
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  NotShared<DOMArrayBufferView> buffer_view = data;
  return ValidateAndCreate(nullptr, &width, nullptr, &buffer_view, nullptr,
                           &exception_state);
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  NotShared<DOMArrayBufferView> buffer_view = data;
  return ValidateAndCreate(nullptr, &width, &height, &buffer_view, nullptr,
                           &exception_state);
}

ImageData* ImageData::CreateImageData(unsigned width,
                                      unsigned height,
                                      const ImageDataSettings* settings,
                                      ExceptionState& exception_state) {
  return ValidateAndCreate(nullptr, &width, &height, nullptr, settings,
                           &exception_state);
}

ImageData* ImageData::CreateImageData(ImageDataArray& data,
                                      unsigned width,
                                      unsigned height,
                                      ImageDataSettings* settings,
                                      ExceptionState& exception_state) {
  NotShared<DOMArrayBufferView> buffer_view;

  // When pixels data is provided, we need to override the storage format of
  // ImageDataSettings with the one that matches the data type of the
  // pixels.
  String storage_format_name;

  if (data.IsUint8ClampedArray()) {
    buffer_view = data.GetAsUint8ClampedArray();
    storage_format_name = kUint8ClampedArrayStorageFormatName;
  } else if (data.IsUint16Array()) {
    buffer_view = data.GetAsUint16Array();
    storage_format_name = kUint16ArrayStorageFormatName;
  } else if (data.IsFloat32Array()) {
    buffer_view = data.GetAsFloat32Array();
    storage_format_name = kFloat32ArrayStorageFormatName;
  } else {
    NOTREACHED();
  }

  if (settings->storageFormat() != storage_format_name)
    settings->setStorageFormat(storage_format_name);

  return ValidateAndCreate(nullptr, &width, &height, &buffer_view, settings,
                           &exception_state);
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

v8::Local<v8::Object> ImageData::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type, wrapper);

  if (!wrapper.IsEmpty() && data_.IsUint8ClampedArray()) {
    // Create a V8 object with |data_| and set the "data" property
    // of the ImageData object to the created v8 object, eliminating the
    // C++ callback when accessing the "data" property.

    v8::Local<v8::Value> pixel_array = ToV8(data_, wrapper, isolate);
    bool defined_property;
    if (pixel_array.IsEmpty() ||
        !wrapper
             ->DefineOwnProperty(isolate->GetCurrentContext(),
                                 V8AtomicString(isolate, "data"), pixel_array,
                                 v8::ReadOnly)
             .To(&defined_property) ||
        !defined_property)
      return v8::Local<v8::Object>();
  }
  return wrapper;
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
      DCHECK(data->GetType() ==
             DOMArrayBufferView::ViewType::kTypeUint8Clamped);
      data_u8_ = data;
      DCHECK(data_u8_);
      data_.SetUint8ClampedArray(data_u8_);
      SECURITY_CHECK(
          (base::CheckedNumeric<size_t>(size.Width()) * size.Height() * 4)
              .ValueOrDie() <= data_.GetAsUint8ClampedArray()->length());
      break;

    case kUint16ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint16);
      data_u16_ = data;
      DCHECK(data_u16_);
      data_.SetUint16Array(data_u16_);
      SECURITY_CHECK(
          (base::CheckedNumeric<size_t>(size.Width()) * size.Height() * 4)
              .ValueOrDie() <= data_.GetAsUint16Array()->length());
      break;

    case kFloat32ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
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
