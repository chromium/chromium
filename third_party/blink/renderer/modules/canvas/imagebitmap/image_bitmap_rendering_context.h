// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class ImageBitmap;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public ImageBitmapRenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextImageBitmap;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  ImageBitmapRenderingContext(CanvasRenderingContextHost*,
                              const CanvasContextCreationAttributesCore&);

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  // CanvasRenderingContext implementation
  ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextImageBitmap;
  }
  ImageBitmap* TransferToImageBitmap(ScriptState*) override;

  void SetCanvasGetContextResult(RenderingContext&) final;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

  ~ImageBitmapRenderingContext() override;
};

DEFINE_TYPE_CASTS(ImageBitmapRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap,
                  context.GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap);

}  // namespace blink

#endif
