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
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data_settings.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
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

constexpr const char* kUint8ClampedArrayStorageFormatName = "uint8";
constexpr const char* kUint16ArrayStorageFormatName = "uint16";
constexpr const char* kFloat32ArrayStorageFormatName = "float32";

class CORE_EXPORT ImageData final : public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor that takes width, height, and an optional ImageDataSettings.
  static ImageData* Create(unsigned width,
                           unsigned height,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, base::nullopt, nullptr,
                             exception_state);
  }
  static ImageData* Create(unsigned width,
                           unsigned height,
                           const ImageDataSettings* settings,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, base::nullopt, settings,
                             exception_state, RequireCanvasColorManagement);
  }

  // Constructor that takes Uint8ClampedArray, width, optional height, and
  // optional ImageDataSettings.
  static ImageData* Create(NotShared<DOMUint8ClampedArray> data,
                           unsigned width,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, base::nullopt, data, nullptr,
                             exception_state);
  }
  static ImageData* Create(NotShared<DOMUint8ClampedArray> data,
                           unsigned width,
                           unsigned height,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, nullptr, exception_state);
  }
  static ImageData* Create(NotShared<DOMUint8ClampedArray> data,
                           unsigned width,
                           unsigned height,
                           const ImageDataSettings* settings,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, settings, exception_state,
                             RequireCanvasColorManagement);
  }

  // Constructor that takes DOMUint16Array, width, optional height, and optional
  // ImageDataSettings.
  static ImageData* Create(NotShared<DOMUint16Array> data,
                           unsigned width,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, base::nullopt, data, nullptr,
                             exception_state, RequireCanvasColorManagement);
  }
  static ImageData* Create(NotShared<DOMUint16Array> data,
                           unsigned width,
                           unsigned height,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, nullptr, exception_state,
                             RequireCanvasColorManagement);
  }
  static ImageData* Create(NotShared<DOMUint16Array> data,
                           unsigned width,
                           unsigned height,
                           const ImageDataSettings* settings,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, settings, exception_state,
                             RequireCanvasColorManagement);
  }

  // Constructor that takes DOMFloat32Array, width, optional height, and
  // optional ImageDataSettings.
  static ImageData* Create(NotShared<DOMFloat32Array> data,
                           unsigned width,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, base::nullopt, data, nullptr,
                             exception_state, RequireCanvasColorManagement);
  }
  static ImageData* Create(NotShared<DOMFloat32Array> data,
                           unsigned width,
                           unsigned height,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, nullptr, exception_state,
                             RequireCanvasColorManagement);
  }
  static ImageData* Create(NotShared<DOMFloat32Array> data,
                           unsigned width,
                           unsigned height,
                           const ImageDataSettings* settings,
                           ExceptionState& exception_state) {
    return ValidateAndCreate(width, height, data, settings, exception_state,
                             RequireCanvasColorManagement);
  }

  // ValidateAndCreate is the common path that all ImageData creation code
  // should call directly. The other Create functions are to be called only by
  // generated code.
  enum ValidateAndCreateFlags {
    None = 0x0,
    // When a too-large ImageData is created using a constructor, it has
    // historically thrown an IndexSizeError. When created through a 2D
    // canvas, it has historically thrown a RangeError. This flag will
    // trigger the RangeError path.
    Context2DErrorMode = 0x1,
    // Constructors in IDL files cannot specify RuntimeEnabled restrictions.
    // This argument is passed by Create functions that should require that the
    // CanvasColorManagement feature be enabled.
    RequireCanvasColorManagement = 0x2,
  };
  static ImageData* ValidateAndCreate(
      unsigned width,
      base::Optional<unsigned> height,
      base::Optional<NotShared<DOMArrayBufferView>> data,
      const ImageDataSettings* settings,
      ExceptionState& exception_state,
      uint32_t flags = 0);

  ImageDataSettings* getSettings() { return settings_; }

  static ImageData* CreateForTest(const IntSize&);
  static ImageData* CreateForTest(const IntSize&,
                                  NotShared<DOMArrayBufferView>,
                                  const ImageDataSettings* = nullptr);

  ImageData(const IntSize&,
            NotShared<DOMArrayBufferView>,
            const ImageDataSettings* = nullptr);

  static String CanvasColorSpaceName(CanvasColorSpace);
  static ImageDataStorageFormat GetImageDataStorageFormat(const String&);
  static unsigned StorageFormatBytesPerPixel(const String&);
  static unsigned StorageFormatBytesPerPixel(ImageDataStorageFormat);

  IntSize Size() const { return size_; }
  int width() const { return size_.Width(); }
  int height() const { return size_.Height(); }

  ImageDataArray& data() { return data_; }
  const ImageDataArray& data() const { return data_; }
  void data(ImageDataArray& result) { result = data_; }

  bool IsBufferBaseDetached() const;
  CanvasColorSpace GetCanvasColorSpace() const;
  ImageDataStorageFormat GetImageDataStorageFormat() const;

  // Return an SkPixmap that references this data directly.
  SkPixmap GetSkPixmap() const;

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return size_; }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  void Trace(Visitor*) const override;

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

 private:
  IntSize size_;
  Member<ImageDataSettings> settings_;
  ImageDataArray data_;
  NotShared<DOMUint8ClampedArray> data_u8_;
  NotShared<DOMUint16Array> data_u16_;
  NotShared<DOMFloat32Array> data_f32_;

  static NotShared<DOMArrayBufferView> AllocateAndValidateDataArray(
      const unsigned&,
      ImageDataStorageFormat,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_
