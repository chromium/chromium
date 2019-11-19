/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/uint8_clamped_array_or_uint16_array_or_float32_array.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/image_data_color_settings.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace blink {

class ExceptionState;
class ImageBitmapOptions;

typedef Uint8ClampedArrayOrUint16ArrayOrFloat32Array ImageDataArray;

enum ConstructorParams {
  kParamSize = 1,
  kParamWidth = 1 << 1,
  kParamHeight = 1 << 2,
  kParamData = 1 << 3,
};

constexpr const char* kUint8ClampedArrayStorageFormatName = "uint8";
constexpr const char* kUint16ArrayStorageFormatName = "uint16";
constexpr const char* kFloat32ArrayStorageFormatName = "float32";

class CORE_EXPORT ImageData final : public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageData* Create(const IntSize&,
                           const ImageDataColorSettings* = nullptr);
  static ImageData* Create(const IntSize&, const CanvasColorParams&);
  static ImageData* Create(const IntSize&,
                           CanvasColorSpace,
                           ImageDataStorageFormat);
  static ImageData* Create(const IntSize&,
                           NotShared<DOMArrayBufferView>,
                           const ImageDataColorSettings* = nullptr);
  static ImageData* Create(scoped_refptr<StaticBitmapImage>,
                           AlphaDisposition = kDontChangeAlpha);

  static ImageData* Create(unsigned width, unsigned height, ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           unsigned height,
                           ExceptionState&);

  static ImageData* CreateImageData(unsigned width,
                                    unsigned height,
                                    const ImageDataColorSettings*,
                                    ExceptionState&);
  static ImageData* CreateImageData(ImageDataArray&,
                                    unsigned width,
                                    unsigned height,
                                    ImageDataColorSettings*,
                                    ExceptionState&);

  ImageDataColorSettings* getColorSettings() { return color_settings_; }

  static ImageData* CreateForTest(const IntSize&);
  static ImageData* CreateForTest(const IntSize&,
                                  DOMArrayBufferView*,
                                  const ImageDataColorSettings* = nullptr);

  ImageData(const IntSize&,
            DOMArrayBufferView*,
            const ImageDataColorSettings* = nullptr);

  ImageData* CropRect(const IntRect&, bool flip_y = false);

  ImageDataStorageFormat GetImageDataStorageFormat();
  static CanvasColorSpace GetCanvasColorSpace(const String&);
  static String CanvasColorSpaceName(CanvasColorSpace);
  static ImageDataStorageFormat GetImageDataStorageFormat(const String&);
  static unsigned StorageFormatDataSize(const String&);
  static unsigned StorageFormatDataSize(ImageDataStorageFormat);
  static DOMArrayBufferView*
  ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      ArrayBufferContents&,
      CanvasPixelFormat,
      ImageDataStorageFormat);

  IntSize Size() const { return size_; }
  int width() const { return size_.Width(); }
  int height() const { return size_.Height(); }

  DOMUint8ClampedArray* data();
  const DOMUint8ClampedArray* data() const;
  ImageDataArray& dataUnion() { return data_union_; }
  const ImageDataArray& dataUnion() const { return data_union_; }
  void dataUnion(ImageDataArray& result) { result = data_union_; }

  DOMArrayBufferBase* BufferBase() const;
  CanvasColorParams GetCanvasColorParams();

  // DataU8ColorType param specifies if the converted pixels in uint8 pixel
  // format should respect the "native" 32bit ARGB format of Skia's blitters.
  // For example, if ImageDataInCanvasColorSettings() is called to fill an
  // ImageBuffer, kRGBAColorType should be used. If the converted pixels are
  // used to create an ImageBitmap, kN32ColorType should be used.
  bool ImageDataInCanvasColorSettings(
      CanvasColorSpace,
      CanvasPixelFormat,
      unsigned char* converted_pixels,
      DataU8ColorType,
      const IntRect* = nullptr,
      const AlphaDisposition = kUnpremultiplyAlpha);

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return size_; }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*) override;

  void Trace(Visitor*) override;

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

  static bool ValidateConstructorArguments(
      const unsigned&,
      const IntSize* = nullptr,
      const unsigned& = 0,
      const unsigned& = 0,
      const DOMArrayBufferView* = nullptr,
      const ImageDataColorSettings* = nullptr,
      ExceptionState* = nullptr);

 private:
  IntSize size_;
  Member<ImageDataColorSettings> color_settings_;
  ImageDataArray data_union_;
  Member<DOMUint8ClampedArray> data_;
  Member<DOMUint16Array> data_u16_;
  Member<DOMFloat32Array> data_f32_;

  static DOMArrayBufferView* AllocateAndValidateDataArray(
      const unsigned&,
      ImageDataStorageFormat,
      ExceptionState* = nullptr);

  static DOMUint8ClampedArray* AllocateAndValidateUint8ClampedArray(
      const unsigned&,
      ExceptionState* = nullptr);

  static DOMUint16Array* AllocateAndValidateUint16Array(
      const unsigned&,
      ExceptionState* = nullptr);

  static DOMFloat32Array* AllocateAndValidateFloat32Array(
      const unsigned&,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_
