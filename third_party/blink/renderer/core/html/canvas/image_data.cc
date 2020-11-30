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

bool RaiseDOMExceptionAndReturnFalse(ExceptionState* exception_state,
                                     DOMExceptionCode exception_code,
                                     const char* message) {
  if (exception_state)
    exception_state->ThrowDOMException(exception_code, message);
  return false;
}

}  // namespace

bool ImageData::ValidateConstructorArguments(
    const unsigned& param_flags,
    const IntSize* size,
    const unsigned& width,
    const unsigned& height,
    const NotShared<DOMArrayBufferView> data,
    const ImageDataColorSettings* color_settings,
    ExceptionState* exception_state) {
  // We accept all the combinations of colorSpace and storageFormat in an
  // ImageDataColorSettings to be stored in an ImageData. Therefore, we don't
  // check the color settings in this function.

  if ((param_flags & kParamWidth) && !width) {
    return RaiseDOMExceptionAndReturnFalse(
        exception_state, DOMExceptionCode::kIndexSizeError,
        "The source width is zero or not a number.");
  }

  if ((param_flags & kParamHeight) && !height) {
    return RaiseDOMExceptionAndReturnFalse(
        exception_state, DOMExceptionCode::kIndexSizeError,
        "The source height is zero or not a number.");
  }

  if (param_flags & (kParamWidth | kParamHeight)) {
    base::CheckedNumeric<unsigned> data_size =
        ImageData::StorageFormatBytesPerPixel(
            kUint8ClampedArrayStorageFormatName);
    if (color_settings) {
      data_size = ImageData::StorageFormatBytesPerPixel(
          color_settings->storageFormat());
    }
    data_size *= width;
    data_size *= height;
    if (!data_size.IsValid()) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The requested image size exceeds the supported range.");
    }

    if (data_size.ValueOrDie() > v8::TypedArray::kMaxLength) {
      if (exception_state) {
        exception_state->ThrowRangeError(
            "Out of memory at ImageData creation.");
      }
      return false;
    }
  }

  unsigned data_length = 0;
  if (param_flags & kParamData) {
    DCHECK(data);
    if (data->GetType() != DOMArrayBufferView::ViewType::kTypeUint8Clamped &&
        data->GetType() != DOMArrayBufferView::ViewType::kTypeUint16 &&
        data->GetType() != DOMArrayBufferView::ViewType::kTypeFloat32) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The input data type is not supported.");
    }

    static_assert(
        std::numeric_limits<unsigned>::max() >=
            std::numeric_limits<uint32_t>::max(),
        "We use UINT32_MAX as the upper bound of the input size and expect "
        "that the result fits into an `unsigned`.");
    if (!base::CheckedNumeric<uint32_t>(data->byteLength())
             .AssignIfValid(&data_length)) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The input data is too large. The maximum size is 4294967295.");
    }
    if (!data_length) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "The input data has zero elements.");
    }
    data_length /= data->TypeSize();
    if (data_length % 4) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "The input data length is not a multiple of 4.");
    }

    if ((param_flags & kParamWidth) && (data_length / 4) % width) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The input data length is not a multiple of (4 * width).");
    }

    if ((param_flags & kParamWidth) && (param_flags & kParamHeight) &&
        height != data_length / (4 * width))
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The input data length is not equal to (4 * width * height).");
  }

  if (param_flags & kParamSize) {
    if (size->Width() <= 0 || size->Height() <= 0)
      return false;
    base::CheckedNumeric<unsigned> data_size = 4;
    data_size *= size->Width();
    data_size *= size->Height();
    if (!data_size.IsValid() ||
        data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
      return false;
    if (param_flags & kParamData) {
      if (data_size.ValueOrDie() > data_length)
        return false;
    }
  }

  return true;
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

NotShared<DOMUint8ClampedArray> ImageData::AllocateAndValidateUint8ClampedArray(
    const unsigned& length,
    ExceptionState* exception_state) {
  NotShared<DOMUint8ClampedArray> buffer_view;
  buffer_view = AllocateAndValidateDataArray(
      length, kUint8ClampedArrayStorageFormat, exception_state);
  return buffer_view;
}

NotShared<DOMUint16Array> ImageData::AllocateAndValidateUint16Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  NotShared<DOMUint16Array> buffer_view;
  buffer_view = AllocateAndValidateDataArray(length, kUint16ArrayStorageFormat,
                                             exception_state);
  return buffer_view;
}

NotShared<DOMFloat32Array> ImageData::AllocateAndValidateFloat32Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  NotShared<DOMFloat32Array> buffer_view;
  buffer_view = AllocateAndValidateDataArray(length, kFloat32ArrayStorageFormat,
                                             exception_state);
  return buffer_view;
}

ImageData* ImageData::Create(const IntSize& size,
                             const ImageDataColorSettings* color_settings) {
  if (!ValidateConstructorArguments(kParamSize, &size, 0, 0,
                                    NotShared<DOMArrayBufferView>(),
                                    color_settings))
    return nullptr;
  ImageDataStorageFormat storage_format = kUint8ClampedArrayStorageFormat;
  if (color_settings) {
    storage_format =
        ImageData::GetImageDataStorageFormat(color_settings->storageFormat());
  }
  NotShared<DOMArrayBufferView> data_array =
      AllocateAndValidateDataArray(4 * static_cast<unsigned>(size.Width()) *
                                       static_cast<unsigned>(size.Height()),
                                   storage_format);
  return data_array
             ? MakeGarbageCollected<ImageData>(size, data_array, color_settings)
             : nullptr;
}

ImageData* ImageData::Create(const IntSize& size,
                             CanvasColorSpace color_space,
                             ImageDataStorageFormat storage_format) {
  ImageDataColorSettings* color_settings = ImageDataColorSettings::Create();
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      color_settings->setColorSpace(kSRGBCanvasColorSpaceName);
      break;
    case CanvasColorSpace::kRec2020:
      color_settings->setColorSpace(kRec2020CanvasColorSpaceName);
      break;
    case CanvasColorSpace::kP3:
      color_settings->setColorSpace(kP3CanvasColorSpaceName);
      break;
  }

  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      color_settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
      break;
    case kUint16ArrayStorageFormat:
      color_settings->setStorageFormat(kUint16ArrayStorageFormatName);
      break;
    case kFloat32ArrayStorageFormat:
      color_settings->setStorageFormat(kFloat32ArrayStorageFormatName);
      break;
  }

  return ImageData::Create(size, color_settings);
}

ImageData* ImageData::Create(const IntSize& size,
                             NotShared<DOMArrayBufferView> data_array,
                             const ImageDataColorSettings* color_settings) {
  if (!ImageData::ValidateConstructorArguments(
          kParamSize | kParamData, &size, 0, 0, data_array, color_settings))
    return nullptr;
  return MakeGarbageCollected<ImageData>(size, data_array, color_settings);
}

ImageData* ImageData::Create(unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamWidth | kParamHeight, nullptr, width, height,
          NotShared<DOMArrayBufferView>(), nullptr, &exception_state))
    return nullptr;

  NotShared<DOMUint8ClampedArray> byte_array =
      AllocateAndValidateUint8ClampedArray(
          ImageData::StorageFormatBytesPerPixel(
              kUint8ClampedArrayStorageFormat) *
              width * height,
          &exception_state);
  return byte_array ? MakeGarbageCollected<ImageData>(IntSize(width, height),
                                                      byte_array)
                    : nullptr;
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data, nullptr,
                                               &exception_state))
    return nullptr;

  unsigned height = base::checked_cast<unsigned>(data->length()) / (width * 4);
  return MakeGarbageCollected<ImageData>(IntSize(width, height), data);
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height, data,
          nullptr, &exception_state))
    return nullptr;

  return MakeGarbageCollected<ImageData>(IntSize(width, height), data);
}

ImageData* ImageData::Create(NotShared<DOMUint16Array> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data, nullptr,
                                               &exception_state))
    return nullptr;
  unsigned height = base::checked_cast<unsigned>(data->length()) /
                    (width * ImageData::StorageFormatBytesPerPixel(
                                 kUint16ArrayStorageFormatName));
  ImageDataColorSettings* image_setting = ImageDataColorSettings::Create();
  image_setting->setStorageFormat(kUint16ArrayStorageFormatName);
  return MakeGarbageCollected<ImageData>(IntSize(width, height), data,
                                         image_setting);
}

ImageData* ImageData::Create(NotShared<DOMUint16Array> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height, data,
          nullptr, &exception_state))
    return nullptr;

  ImageDataColorSettings* image_setting = ImageDataColorSettings::Create();
  image_setting->setStorageFormat(kUint16ArrayStorageFormatName);
  return MakeGarbageCollected<ImageData>(IntSize(width, height), data,
                                         image_setting);
}

ImageData* ImageData::Create(NotShared<DOMFloat32Array> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data, nullptr,
                                               &exception_state))
    return nullptr;

  unsigned height = base::checked_cast<unsigned>(data->length()) /
                    (width * ImageData::StorageFormatBytesPerPixel(
                                 kFloat32ArrayStorageFormatName));
  ImageDataColorSettings* image_setting = ImageDataColorSettings::Create();
  image_setting->setStorageFormat(kFloat32ArrayStorageFormatName);
  return MakeGarbageCollected<ImageData>(IntSize(width, height), data,
                                         image_setting);
}

ImageData* ImageData::Create(NotShared<DOMFloat32Array> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height, data,
          nullptr, &exception_state))
    return nullptr;

  ImageDataColorSettings* image_setting = ImageDataColorSettings::Create();
  image_setting->setStorageFormat(kFloat32ArrayStorageFormatName);
  return MakeGarbageCollected<ImageData>(IntSize(width, height), data,
                                         image_setting);
}

ImageData* ImageData::CreateImageData(
    unsigned width,
    unsigned height,
    const ImageDataColorSettings* color_settings,
    ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamWidth | kParamHeight, nullptr, width, height,
          NotShared<DOMArrayBufferView>(), color_settings, &exception_state))
    return nullptr;

  ImageDataStorageFormat storage_format =
      ImageData::GetImageDataStorageFormat(color_settings->storageFormat());
  NotShared<DOMArrayBufferView> buffer_view = AllocateAndValidateDataArray(
      4 * width * height, storage_format, &exception_state);

  if (!buffer_view)
    return nullptr;

  return MakeGarbageCollected<ImageData>(IntSize(width, height), buffer_view,
                                         color_settings);
}

ImageData* ImageData::CreateImageData(ImageDataArray& data,
                                      unsigned width,
                                      unsigned height,
                                      ImageDataColorSettings* color_settings,
                                      ExceptionState& exception_state) {
  NotShared<DOMArrayBufferView> buffer_view;

  // When pixels data is provided, we need to override the storage format of
  // ImageDataColorSettings with the one that matches the data type of the
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

  if (color_settings->storageFormat() != storage_format_name)
    color_settings->setStorageFormat(storage_format_name);

  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          buffer_view, color_settings, &exception_state))
    return nullptr;

  return MakeGarbageCollected<ImageData>(IntSize(width, height), buffer_view,
                                         color_settings);
}

// This function accepts size (0, 0) and always returns the ImageData in
// "srgb" color space and "uint8" storage format.
ImageData* ImageData::CreateForTest(const IntSize& size) {
  base::CheckedNumeric<unsigned> data_size =
      ImageData::StorageFormatBytesPerPixel(kUint8ClampedArrayStorageFormat);
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
ImageData* ImageData::CreateForTest(
    const IntSize& size,
    NotShared<DOMArrayBufferView> buffer_view,
    const ImageDataColorSettings* color_settings) {
  return MakeGarbageCollected<ImageData>(size, buffer_view, color_settings);
}

// Crops ImageData to the intersect of its size and the given rectangle. If the
// intersection is empty or it cannot create the cropped ImageData it returns
// nullptr. This function leaves the source ImageData intact. When crop_rect
// covers all the ImageData, a copy of the ImageData is returned.
// TODO (zakerinasab): crbug.com/774484: As a rule of thumb ImageData belongs to
// the user and its state should not change unless directly modified by the
// user. Therefore, we should be able to remove the extra copy and return a
// "cropped view" on the source ImageData object.
ImageData* ImageData::CropRect(const IntRect& crop_rect, bool flip_y) {
  IntRect src_rect(IntPoint(), size_);
  const IntRect dst_rect = Intersection(src_rect, crop_rect);
  if (dst_rect.IsEmpty())
    return nullptr;

  unsigned data_size = 4 * dst_rect.Width() * dst_rect.Height();
  NotShared<DOMArrayBufferView> buffer_view = AllocateAndValidateDataArray(
      data_size,
      ImageData::GetImageDataStorageFormat(color_settings_->storageFormat()));
  if (!buffer_view)
    return nullptr;

  if (src_rect == dst_rect && !flip_y) {
    std::memcpy(buffer_view->BufferBase()->Data(), BufferBase()->Data(),
                data_size * buffer_view->TypeSize());
  } else {
    unsigned data_type_size =
        ImageData::StorageFormatBytesPerPixel(color_settings_->storageFormat());
    int src_index = (dst_rect.X() + dst_rect.Y() * src_rect.Width()) * 4;
    int dst_index = 0;
    if (flip_y)
      dst_index = (dst_rect.Height() - 1) * dst_rect.Width() * 4;
    int src_row_stride = src_rect.Width() * 4;
    int dst_row_stride = flip_y ? -dst_rect.Width() * 4 : dst_rect.Width() * 4;
    for (int i = 0; i < dst_rect.Height(); i++) {
      std::memcpy(static_cast<char*>(buffer_view->BufferBase()->Data()) +
                      dst_index / 4 * data_type_size,
                  static_cast<char*>(BufferBase()->Data()) +
                      src_index / 4 * data_type_size,
                  dst_rect.Width() * data_type_size);
      src_index += src_row_stride;
      dst_index += dst_row_stride;
    }
  }
  return MakeGarbageCollected<ImageData>(dst_rect.Size(), buffer_view,
                                         color_settings_);
}

ScriptPromise ImageData::CreateImageBitmap(ScriptState* script_state,
                                           base::Optional<IntRect> crop_rect,
                                           const ImageBitmapOptions* options,
                                           ExceptionState& exception_state) {
  if (BufferBase()->IsDetached()) {
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
  return CanvasColorSpaceFromName(color_settings_->colorSpace());
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

DOMArrayBufferBase* ImageData::BufferBase() const {
  if (data_.IsUint8ClampedArray())
    return data_.GetAsUint8ClampedArray()->BufferBase();
  if (data_.IsUint16Array())
    return data_.GetAsUint16Array()->BufferBase();
  if (data_.IsFloat32Array())
    return data_.GetAsFloat32Array()->BufferBase();
  return nullptr;
}

SkPixmap ImageData::GetSkPixmap() const {
  SkColorType color_type = kRGBA_8888_SkColorType;
  if (data_u16_) {
    color_type = kR16G16B16A16_unorm_SkColorType;
  } else if (data_f32_) {
    color_type = kRGBA_F32_SkColorType;
  }
  SkImageInfo info =
      SkImageInfo::Make(width(), height(), color_type, kUnpremul_SkAlphaType,
                        CanvasColorSpaceToSkColorSpace(GetCanvasColorSpace()));
  return SkPixmap(info, BufferBase()->Data(), info.minRowBytes());
}

void ImageData::Trace(Visitor* visitor) const {
  visitor->Trace(color_settings_);
  visitor->Trace(data_);
  visitor->Trace(data_u8_);
  visitor->Trace(data_u16_);
  visitor->Trace(data_f32_);
  ScriptWrappable::Trace(visitor);
}

ImageData::ImageData(const IntSize& size,
                     NotShared<DOMArrayBufferView> data,
                     const ImageDataColorSettings* color_settings)
    : size_(size), color_settings_(ImageDataColorSettings::Create()) {
  DCHECK_GE(size.Width(), 0);
  DCHECK_GE(size.Height(), 0);
  DCHECK(data);

  data_u8_.Clear();
  data_u16_.Clear();
  data_f32_.Clear();

  if (color_settings) {
    color_settings_->setColorSpace(color_settings->colorSpace());
    color_settings_->setStorageFormat(color_settings->storageFormat());
  }

  ImageDataStorageFormat storage_format =
      GetImageDataStorageFormat(color_settings_->storageFormat());

  // TODO (zakerinasab): crbug.com/779570
  // The default color space for ImageData with U16/F32 data should be
  // extended-srgb color space. It is temporarily set to linear-rgb, which is
  // not correct, but fixes crbug.com/779419.

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
