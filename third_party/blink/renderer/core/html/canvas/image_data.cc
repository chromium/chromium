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
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_clamped_array.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/skia/third_party/skcms/skcms.h"
#include "v8/include/v8.h"

namespace blink {

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
    const DOMArrayBufferView* data,
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
    base::CheckedNumeric<unsigned> data_size = 4;
    if (color_settings) {
      data_size *=
          ImageData::StorageFormatDataSize(color_settings->storageFormat());
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

    if (!data->byteLength()) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kIndexSizeError,
          "The input data has zero elements.");
    }

    data_length = data->byteLength() / data->TypeSize();
    if (data_length % 4) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, DOMExceptionCode::kIndexSizeError,
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

DOMArrayBufferView* ImageData::AllocateAndValidateDataArray(
    const unsigned& length,
    ImageDataStorageFormat storage_format,
    ExceptionState* exception_state) {
  if (!length)
    return nullptr;

  DOMArrayBufferView* data_array = nullptr;
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      data_array = DOMUint8ClampedArray::CreateOrNull(length);
      break;
    case kUint16ArrayStorageFormat:
      data_array = DOMUint16Array::CreateOrNull(length);
      break;
    case kFloat32ArrayStorageFormat:
      data_array = DOMFloat32Array::CreateOrNull(length);
      break;
    default:
      NOTREACHED();
  }

  if (!data_array ||
      length != data_array->byteLength() / data_array->TypeSize()) {
    if (exception_state)
      exception_state->ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  return data_array;
}

DOMUint8ClampedArray* ImageData::AllocateAndValidateUint8ClampedArray(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kUint8ClampedArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMUint8ClampedArray* u8_array = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(buffer_view));
  DCHECK(u8_array);
  return u8_array;
}

DOMUint16Array* ImageData::AllocateAndValidateUint16Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kUint16ArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMUint16Array* u16_array = const_cast<DOMUint16Array*>(
      static_cast<const DOMUint16Array*>(buffer_view));
  DCHECK(u16_array);
  return u16_array;
}

DOMFloat32Array* ImageData::AllocateAndValidateFloat32Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kFloat32ArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMFloat32Array* f32_array = const_cast<DOMFloat32Array*>(
      static_cast<const DOMFloat32Array*>(buffer_view));
  DCHECK(f32_array);
  return f32_array;
}

ImageData* ImageData::Create(const IntSize& size,
                             const ImageDataColorSettings* color_settings) {
  if (!ImageData::ValidateConstructorArguments(kParamSize, &size, 0, 0, nullptr,
                                               color_settings))
    return nullptr;
  ImageDataStorageFormat storage_format = kUint8ClampedArrayStorageFormat;
  if (color_settings) {
    storage_format =
        ImageData::GetImageDataStorageFormat(color_settings->storageFormat());
  }
  DOMArrayBufferView* data_array =
      AllocateAndValidateDataArray(4 * static_cast<unsigned>(size.Width()) *
                                       static_cast<unsigned>(size.Height()),
                                   storage_format);
  return data_array ? new ImageData(size, data_array, color_settings) : nullptr;
}

ImageDataColorSettings CanvasColorParamsToImageDataColorSettings(
    const CanvasColorParams& color_params) {
  ImageDataColorSettings color_settings;
  switch (color_params.ColorSpace()) {
    case kSRGBCanvasColorSpace:
      color_settings.setColorSpace(kSRGBCanvasColorSpaceName);
      break;
    case kLinearRGBCanvasColorSpace:
      color_settings.setColorSpace(kLinearRGBCanvasColorSpaceName);
      break;
    case kRec2020CanvasColorSpace:
      color_settings.setColorSpace(kRec2020CanvasColorSpaceName);
      break;
    case kP3CanvasColorSpace:
      color_settings.setColorSpace(kP3CanvasColorSpaceName);
      break;
  }
  color_settings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
  if (color_params.PixelFormat() == kF16CanvasPixelFormat)
    color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
  return color_settings;
}

ImageData* ImageData::Create(const IntSize& size,
                             const CanvasColorParams& color_params) {
  ImageDataColorSettings color_settings =
      CanvasColorParamsToImageDataColorSettings(color_params);
  return ImageData::Create(size, &color_settings);
}

ImageData* ImageData::Create(const IntSize& size,
                             CanvasColorSpace color_space,
                             ImageDataStorageFormat storage_format) {
  ImageDataColorSettings color_settings;
  switch (color_space) {
    case kSRGBCanvasColorSpace:
      color_settings.setColorSpace(kSRGBCanvasColorSpaceName);
      break;
    case kLinearRGBCanvasColorSpace:
      color_settings.setColorSpace(kLinearRGBCanvasColorSpaceName);
      break;
    case kRec2020CanvasColorSpace:
      color_settings.setColorSpace(kRec2020CanvasColorSpaceName);
      break;
    case kP3CanvasColorSpace:
      color_settings.setColorSpace(kP3CanvasColorSpaceName);
      break;
  }

  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      color_settings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
      break;
    case kUint16ArrayStorageFormat:
      color_settings.setStorageFormat(kUint16ArrayStorageFormatName);
      break;
    case kFloat32ArrayStorageFormat:
      color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
      break;
  }

  return ImageData::Create(size, &color_settings);
}

ImageData* ImageData::Create(const IntSize& size,
                             NotShared<DOMArrayBufferView> data_array,
                             const ImageDataColorSettings* color_settings) {
  if (!ImageData::ValidateConstructorArguments(kParamSize | kParamData, &size,
                                               0, 0, data_array.View(),
                                               color_settings))
    return nullptr;
  return new ImageData(size, data_array.View(), color_settings);
}

static SkImageInfo GetImageInfo(scoped_refptr<StaticBitmapImage> image) {
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  return SkImageInfo::Make(skia_image->width(), skia_image->height(),
                           skia_image->colorType(), skia_image->alphaType(),
                           skia_image->refColorSpace());
}

ImageData* ImageData::Create(scoped_refptr<StaticBitmapImage> image,
                             AlphaDisposition alpha_disposition) {
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  DCHECK(skia_image);
  SkImageInfo image_info = GetImageInfo(image);
  CanvasColorParams color_params(image_info);
  if (image_info.alphaType() != kOpaque_SkAlphaType) {
    if (alpha_disposition == kPremultiplyAlpha) {
      image_info = image_info.makeAlphaType(kPremul_SkAlphaType);
    } else if (alpha_disposition == kUnpremultiplyAlpha) {
      image_info = image_info.makeAlphaType(kUnpremul_SkAlphaType);
    }
  }

  SkColorType color_type = image_info.colorType();
  bool create_f32_image_data = (color_type == kRGBA_1010102_SkColorType ||
                                color_type == kRGB_101010x_SkColorType ||
                                color_type == kRGBA_F16_SkColorType ||
                                color_type == kRGBA_F32_SkColorType);

  if (!create_f32_image_data) {
    ImageData* image_data = Create(image->Size(), color_params);
    if (!image_data)
      return nullptr;
    image_info = image_info.makeColorType(kRGBA_8888_SkColorType);
    skia_image->readPixels(image_info, image_data->data()->Data(),
                           image_info.minRowBytes(), 0, 0);
    return image_data;
  }

  // Create image data with f32 storage
  DOMFloat32Array* f32_array = ImageData::AllocateAndValidateFloat32Array(
      image->Size().Area() * 4, nullptr);
  if (!f32_array)
    return nullptr;
  image_info = image_info.makeColorType(kRGBA_F32_SkColorType);
  skia_image->readPixels(image_info, f32_array->Data(),
                         image_info.minRowBytes(), 0, 0);
  ImageDataColorSettings color_settings =
      CanvasColorParamsToImageDataColorSettings(color_params);
  return Create(image->Size(), NotShared<blink::DOMArrayBufferView>(f32_array),
                &color_settings);
}

ImageData* ImageData::Create(unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               nullptr, &exception_state))
    return nullptr;

  DOMArrayBufferView* byte_array = AllocateAndValidateDataArray(
      4 * width * height, kUint8ClampedArrayStorageFormat, &exception_state);
  return byte_array ? new ImageData(IntSize(width, height), byte_array)
                    : nullptr;
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data.View(),
                                               nullptr, &exception_state))
    return nullptr;

  unsigned height = data.View()->length() / (width * 4);
  return new ImageData(IntSize(width, height), data.View());
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          data.View(), nullptr, &exception_state))
    return nullptr;

  return new ImageData(IntSize(width, height), data.View());
}

ImageData* ImageData::CreateImageData(
    unsigned width,
    unsigned height,
    const ImageDataColorSettings& color_settings,
    ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamWidth | kParamHeight, nullptr, width, height, nullptr,
          &color_settings, &exception_state))
    return nullptr;

  ImageDataStorageFormat storage_format =
      ImageData::GetImageDataStorageFormat(color_settings.storageFormat());
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      4 * width * height, storage_format, &exception_state);

  if (!buffer_view)
    return nullptr;

  return new ImageData(IntSize(width, height), buffer_view, &color_settings);
}

ImageData* ImageData::CreateImageData(ImageDataArray& data,
                                      unsigned width,
                                      unsigned height,
                                      ImageDataColorSettings& color_settings,
                                      ExceptionState& exception_state) {
  DOMArrayBufferView* buffer_view = nullptr;

  // When pixels data is provided, we need to override the storage format of
  // ImageDataColorSettings with the one that matches the data type of the
  // pixels.
  String storage_format_name;

  if (data.IsUint8ClampedArray()) {
    buffer_view = data.GetAsUint8ClampedArray().View();
    storage_format_name = kUint8ClampedArrayStorageFormatName;
  } else if (data.IsUint16Array()) {
    buffer_view = data.GetAsUint16Array().View();
    storage_format_name = kUint16ArrayStorageFormatName;
  } else if (data.IsFloat32Array()) {
    buffer_view = data.GetAsFloat32Array().View();
    storage_format_name = kFloat32ArrayStorageFormatName;
  } else {
    NOTREACHED();
  }

  if (storage_format_name != color_settings.storageFormat())
    color_settings.setStorageFormat(storage_format_name);

  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          buffer_view, &color_settings, &exception_state))
    return nullptr;

  return new ImageData(IntSize(width, height), buffer_view, &color_settings);
}

// This function accepts size (0, 0) and always returns the ImageData in
// "srgb" color space and "uint8" storage format.
ImageData* ImageData::CreateForTest(const IntSize& size) {
  base::CheckedNumeric<unsigned> data_size = 4;
  data_size *= size.Width();
  data_size *= size.Height();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return nullptr;

  DOMUint8ClampedArray* byte_array =
      DOMUint8ClampedArray::CreateOrNull(data_size.ValueOrDie());
  if (!byte_array)
    return nullptr;

  return new ImageData(size, byte_array);
}

// This function is called from unit tests, and all the parameters are supposed
// to be validated on the call site.
ImageData* ImageData::CreateForTest(
    const IntSize& size,
    DOMArrayBufferView* buffer_view,
    const ImageDataColorSettings* color_settings) {
  return new ImageData(size, buffer_view, color_settings);
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
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      data_size,
      ImageData::GetImageDataStorageFormat(color_settings_.storageFormat()));
  if (!buffer_view)
    return nullptr;

  if (src_rect == dst_rect && !flip_y) {
    std::memcpy(buffer_view->BufferBase()->Data(), BufferBase()->Data(),
                data_size * buffer_view->TypeSize());
  } else {
    unsigned data_type_size =
        ImageData::StorageFormatDataSize(color_settings_.storageFormat());
    int src_index = (dst_rect.X() + dst_rect.Y() * src_rect.Width()) * 4;
    int dst_index = 0;
    if (flip_y)
      dst_index = (dst_rect.Height() - 1) * dst_rect.Width() * 4;
    int src_row_stride = src_rect.Width() * 4;
    int dst_row_stride = flip_y ? -dst_rect.Width() * 4 : dst_rect.Width() * 4;
    for (int i = 0; i < dst_rect.Height(); i++) {
      std::memcpy(
          static_cast<char*>(buffer_view->BufferBase()->Data()) +
              dst_index * data_type_size,
          static_cast<char*>(BufferBase()->Data()) + src_index * data_type_size,
          dst_rect.Width() * 4 * data_type_size);
      src_index += src_row_stride;
      dst_index += dst_row_stride;
    }
  }
  return new ImageData(dst_rect.Size(), buffer_view, &color_settings_);
}

ScriptPromise ImageData::CreateImageBitmap(ScriptState* script_state,
                                           EventTarget& event_target,
                                           base::Optional<IntRect> crop_rect,
                                           const ImageBitmapOptions& options) {
  if (BufferBase()->IsNeutered()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "The source data has been detached."));
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(this, crop_rect, options));
}

v8::Local<v8::Object> ImageData::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type, wrapper);

  if (!wrapper.IsEmpty() && data_) {
    // Create a V8 Uint8ClampedArray object and set the "data" property
    // of the ImageData object to the created v8 object, eliminating the
    // C++ callback when accessing the "data" property.
    v8::Local<v8::Value> pixel_array = ToV8(data_.Get(), wrapper, isolate);
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

const DOMUint8ClampedArray* ImageData::data() const {
  if (color_settings_.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return data_.Get();
  return nullptr;
}

DOMUint8ClampedArray* ImageData::data() {
  if (color_settings_.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return data_.Get();
  return nullptr;
}

CanvasColorSpace ImageData::GetCanvasColorSpace(
    const String& color_space_name) {
  if (color_space_name == kSRGBCanvasColorSpaceName)
    return kSRGBCanvasColorSpace;
  if (color_space_name == kLinearRGBCanvasColorSpaceName)
    return kLinearRGBCanvasColorSpace;
  if (color_space_name == kRec2020CanvasColorSpaceName)
    return kRec2020CanvasColorSpace;
  if (color_space_name == kP3CanvasColorSpaceName)
    return kP3CanvasColorSpace;
  NOTREACHED();
  return kSRGBCanvasColorSpace;
}

String ImageData::CanvasColorSpaceName(CanvasColorSpace color_space) {
  switch (color_space) {
    case kSRGBCanvasColorSpace:
      return kSRGBCanvasColorSpaceName;
    case kLinearRGBCanvasColorSpace:
      return kLinearRGBCanvasColorSpaceName;
    case kRec2020CanvasColorSpace:
      return kRec2020CanvasColorSpaceName;
    case kP3CanvasColorSpace:
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

ImageDataStorageFormat ImageData::GetImageDataStorageFormat() {
  if (data_u16_)
    return kUint16ArrayStorageFormat;
  if (data_f32_)
    return kFloat32ArrayStorageFormat;
  return kUint8ClampedArrayStorageFormat;
}

unsigned ImageData::StorageFormatDataSize(const String& storage_format_name) {
  if (storage_format_name == kUint8ClampedArrayStorageFormatName)
    return 1;
  if (storage_format_name == kUint16ArrayStorageFormatName)
    return 2;
  if (storage_format_name == kFloat32ArrayStorageFormatName)
    return 4;
  NOTREACHED();
  return 1;
}

unsigned ImageData::StorageFormatDataSize(
    ImageDataStorageFormat storage_format) {
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      return 1;
    case kUint16ArrayStorageFormat:
      return 2;
    case kFloat32ArrayStorageFormat:
      return 4;
  }
  NOTREACHED();
  return 1;
}

DOMArrayBufferView*
ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
    WTF::ArrayBufferContents& content,
    CanvasPixelFormat pixel_format,
    ImageDataStorageFormat storage_format) {
  if (!content.DataLength())
    return nullptr;

  if (pixel_format == kRGBA8CanvasPixelFormat &&
      storage_format == kUint8ClampedArrayStorageFormat) {
    DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(content);
    return DOMUint8ClampedArray::Create(array_buffer, 0,
                                        array_buffer->ByteLength());
  }

  skcms_PixelFormat src_format = skcms_PixelFormat_RGBA_8888;
  unsigned num_pixels = content.DataLength() / 4;
  if (pixel_format == kF16CanvasPixelFormat) {
    src_format = skcms_PixelFormat_RGBA_hhhh;
    num_pixels /= 2;
  }
  skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;

  if (storage_format == kUint8ClampedArrayStorageFormat) {
    DOMUint8ClampedArray* u8_array =
        AllocateAndValidateUint8ClampedArray(num_pixels * 4);
    if (!u8_array)
      return nullptr;
    bool data_transform_successful = skcms_Transform(
        content.Data(), src_format, alpha_format, nullptr, u8_array->Data(),
        skcms_PixelFormat_RGBA_8888, alpha_format, nullptr, num_pixels);
    DCHECK(data_transform_successful);
    return u8_array;
  }

  if (storage_format == kUint16ArrayStorageFormat) {
    DOMUint16Array* u16_array = AllocateAndValidateUint16Array(num_pixels * 4);
    if (!u16_array)
      return nullptr;
    bool data_transform_successful = skcms_Transform(
        content.Data(), src_format, alpha_format, nullptr, u16_array->Data(),
        skcms_PixelFormat_RGBA_16161616LE, alpha_format, nullptr, num_pixels);
    DCHECK(data_transform_successful);
    return u16_array;
  }

  DOMFloat32Array* f32_array = AllocateAndValidateFloat32Array(num_pixels * 4);
  if (!f32_array)
    return nullptr;
  bool data_transform_successful = skcms_Transform(
      content.Data(), src_format, alpha_format, nullptr, f32_array->Data(),
      skcms_PixelFormat_RGBA_ffff, alpha_format, nullptr, num_pixels);
  DCHECK(data_transform_successful);
  return f32_array;
}

DOMArrayBufferBase* ImageData::BufferBase() const {
  if (data_)
    return data_->BufferBase();
  if (data_u16_)
    return data_u16_->BufferBase();
  if (data_f32_)
    return data_f32_->BufferBase();
  return nullptr;
}

CanvasColorParams ImageData::GetCanvasColorParams() {
  if (!RuntimeEnabledFeatures::CanvasColorManagementEnabled())
    return CanvasColorParams();
  CanvasColorSpace color_space =
      ImageData::GetCanvasColorSpace(color_settings_.colorSpace());
  CanvasPixelFormat pixel_format = kRGBA8CanvasPixelFormat;
  if (color_settings_.storageFormat() != kUint8ClampedArrayStorageFormatName)
    pixel_format = kF16CanvasPixelFormat;
  return CanvasColorParams(color_space, pixel_format, kNonOpaque);
}

bool ImageData::ImageDataInCanvasColorSettings(
    CanvasColorSpace canvas_color_space,
    CanvasPixelFormat canvas_pixel_format,
    unsigned char* converted_pixels,
    DataU8ColorType u8_color_type,
    const IntRect* src_rect,
    const AlphaDisposition alpha_disposition) {
  if (!data_ && !data_u16_ && !data_f32_)
    return false;

  CanvasColorParams canvas_color_params =
      CanvasColorParams(canvas_color_space, canvas_pixel_format, kNonOpaque);

  unsigned char* src_data = static_cast<unsigned char*>(BufferBase()->Data());

  ImageDataStorageFormat storage_format = GetImageDataStorageFormat();

  skcms_PixelFormat src_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (data_u16_)
    src_pixel_format = skcms_PixelFormat_RGBA_16161616LE;
  else if (data_f32_)
    src_pixel_format = skcms_PixelFormat_RGBA_ffff;

  skcms_PixelFormat dst_pixel_format = skcms_PixelFormat_RGBA_8888;
  if (canvas_pixel_format == kRGBA8CanvasPixelFormat &&
      u8_color_type == kN32ColorType &&
      kN32_SkColorType == kBGRA_8888_SkColorType) {
    dst_pixel_format = skcms_PixelFormat_BGRA_8888;
  } else if (canvas_pixel_format == kF16CanvasPixelFormat) {
    dst_pixel_format = skcms_PixelFormat_RGBA_hhhh;
  }

  skcms_AlphaFormat src_alpha_format = skcms_AlphaFormat_Unpremul;
  skcms_AlphaFormat dst_alpha_format = skcms_AlphaFormat_Unpremul;
  if (alpha_disposition == kPremultiplyAlpha)
    dst_alpha_format = skcms_AlphaFormat_PremulAsEncoded;

  skcms_ICCProfile* src_profile_ptr = nullptr;
  skcms_ICCProfile* dst_profile_ptr = nullptr;
  skcms_ICCProfile src_profile, dst_profile;
  GetCanvasColorParams().GetSkColorSpace()->toProfile(&src_profile);
  canvas_color_params.GetSkColorSpace()->toProfile(&dst_profile);
  // If the profiles are similar, we better leave them as nullptr, since
  // skcms_Transform() only checks for profile pointer equality for the fast
  // path.
  if (!skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile)) {
    src_profile_ptr = &src_profile;
    dst_profile_ptr = &dst_profile;
  }

  const IntRect* crop_rect = nullptr;
  if (src_rect && *src_rect != IntRect(IntPoint(), Size()))
    crop_rect = src_rect;

  // If only a portion of ImageData is required for canvas, we run the transform
  // for every line.
  if (crop_rect) {
    unsigned bytes_per_pixel =
        ImageData::StorageFormatDataSize(storage_format) * 4;
    unsigned src_index =
        (crop_rect->X() + crop_rect->Y() * width()) * bytes_per_pixel;
    unsigned dst_index = 0;
    unsigned src_row_stride = width() * bytes_per_pixel;
    unsigned dst_row_stride =
        crop_rect->Width() * canvas_color_params.BytesPerPixel();
    bool data_transform_successful = true;

    for (int i = 0; data_transform_successful && i < crop_rect->Height(); i++) {
      data_transform_successful = skcms_Transform(
          src_data + src_index, src_pixel_format, src_alpha_format,
          src_profile_ptr, converted_pixels + dst_index, dst_pixel_format,
          dst_alpha_format, dst_profile_ptr, crop_rect->Width());
      DCHECK(data_transform_successful);
      src_index += src_row_stride;
      dst_index += dst_row_stride;
    }
    return data_transform_successful;
  }

  bool data_transform_successful =
      skcms_Transform(src_data, src_pixel_format, src_alpha_format,
                      src_profile_ptr, converted_pixels, dst_pixel_format,
                      dst_alpha_format, dst_profile_ptr, size_.Area());
  return data_transform_successful;
}

void ImageData::Trace(blink::Visitor* visitor) {
  visitor->Trace(data_);
  visitor->Trace(data_u16_);
  visitor->Trace(data_f32_);
  visitor->Trace(data_union_);
  ScriptWrappable::Trace(visitor);
}

ImageData::ImageData(const IntSize& size,
                     DOMArrayBufferView* data,
                     const ImageDataColorSettings* color_settings)
    : size_(size) {
  DCHECK_GE(size.Width(), 0);
  DCHECK_GE(size.Height(), 0);
  DCHECK(data);

  data_ = nullptr;
  data_u16_ = nullptr;
  data_f32_ = nullptr;

  if (color_settings) {
    color_settings_.setColorSpace(color_settings->colorSpace());
    color_settings_.setStorageFormat(color_settings->storageFormat());
  }

  ImageDataStorageFormat storage_format =
      GetImageDataStorageFormat(color_settings_.storageFormat());

  // TODO (zakerinasab): crbug.com/779570
  // The default color space for ImageData with U16/F32 data should be
  // extended-srgb color space. It is temporarily set to linear-rgb, which is
  // not correct, but fixes crbug.com/779419.

  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      DCHECK(data->GetType() ==
             DOMArrayBufferView::ViewType::kTypeUint8Clamped);
      data_ = const_cast<DOMUint8ClampedArray*>(
          static_cast<const DOMUint8ClampedArray*>(data));
      DCHECK(data_);
      data_union_.SetUint8ClampedArray(data_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_->length());
      break;

    case kUint16ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint16);
      data_u16_ =
          const_cast<DOMUint16Array*>(static_cast<const DOMUint16Array*>(data));
      DCHECK(data_u16_);
      data_union_.SetUint16Array(data_u16_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_u16_->length());
      break;

    case kFloat32ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
      data_f32_ = const_cast<DOMFloat32Array*>(
          static_cast<const DOMFloat32Array*>(data));
      DCHECK(data_f32_);
      data_union_.SetFloat32Array(data_f32_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_f32_->length());
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace blink
