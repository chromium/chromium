// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_

#include <memory>
#include <random>

#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

class CanvasResourceProvider;
class Font;
class TextMetrics;

class MODULES_EXPORT OffscreenCanvasRenderingContext2D final
    : public CanvasRenderingContext,
      public BaseRenderingContext2D {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost* host,
        const CanvasContextCreationAttributesCore& attrs) override;

    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContext2D;
    }
  };

  OffscreenCanvasRenderingContext2D(
      OffscreenCanvas*,
      const CanvasContextCreationAttributesCore& attrs);

  OffscreenCanvas* offscreenCanvasForBinding() const {
    DCHECK(!Host() || Host()->IsOffscreenCanvas());
    return static_cast<OffscreenCanvas*>(Host());
  }

  void commit();

  // CanvasRenderingContext implementation
  ~OffscreenCanvasRenderingContext2D() override;
  ContextType GetContextType() const override { return kContext2D; }
  bool IsRenderingContext2D() const override { return true; }
  bool IsComposited() const override { return false; }
  bool IsAccelerated() const override;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;
  void SetIsInHiddenPage(bool) final { NOTREACHED(); }
  void SetIsBeingDisplayed(bool) final { NOTREACHED(); }
  void Stop() final { NOTREACHED(); }
  void SetCanvasGetContextResult(RenderingContext&) final {}
  void ClearRect(double x, double y, double width, double height) override {
    BaseRenderingContext2D::clearRect(x, y, width, height);
  }
  scoped_refptr<StaticBitmapImage> GetImage() final;
  void Reset() override;
  void RestoreCanvasMatrixClipStack(cc::PaintCanvas* c) const override {
    RestoreMatrixClipStack(c);
  }

  String font() const;
  void setFont(const String&) override;

  String direction() const;
  void setDirection(const String&);

  void setTextLetterSpacing(const double letter_spacing);
  void setTextWordSpacing(const double word_spacing);
  void setTextRendering(const String&);
  void setFontKerning(const String&);
  void setFontVariantCaps(const String&);

  void fillText(const String& text, double x, double y);
  void fillText(const String& text, double x, double y, double max_width);
  void strokeText(const String& text, double x, double y);
  void strokeText(const String& text, double x, double y, double max_width);
  TextMetrics* measureText(const String& text);

  // BaseRenderingContext2D implementation
  bool OriginClean() const final;
  void SetOriginTainted() final;
  bool WouldTaintOrigin(CanvasImageSource*) final;

  int Width() const final;
  int Height() const final;

  bool CanCreateCanvas2dResourceProvider() const final;
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider() const;
  CanvasResourceProvider* GetCanvasResourceProvider() const;

  // Offscreen canvas doesn't have any notion of image orientation.
  RespectImageOrientationEnum RespectImageOrientation() const final {
    return kRespectImageOrientation;
  }

  bool ParseColorOrCurrentColor(Color&, const String& color_string) const final;

  cc::PaintCanvas* GetOrCreatePaintCanvas() final;
  cc::PaintCanvas* GetPaintCanvas() const final;

  void DidDraw() final;
  void DidDraw(const SkIRect& dirty_rect) final;

  bool StateHasFilter() final;
  sk_sp<PaintFilter> StateGetFilter() final;
  void SnapshotStateForFilter() final;

  void ValidateStateStackWithCanvas(const cc::PaintCanvas*) const final;

  bool HasAlpha() const final { return CreationAttributes().alpha; }
  bool IsDesynchronized() const final {
    return CreationAttributes().desynchronized;
  }
  bool isContextLost() const override;

  ImageBitmap* TransferToImageBitmap(ScriptState*) final;

  void Trace(Visitor*) const override;

  bool PushFrame() override;

  IdentifiableToken IdentifiableTextToken() const override {
    return identifiability_study_helper_.GetToken();
  }

  bool IdentifiabilityEncounteredSkippedOps() const override {
    return identifiability_study_helper_.encountered_skipped_ops();
  }

  bool IdentifiabilityEncounteredSensitiveOps() const override {
    return identifiability_study_helper_.encountered_sensitive_ops();
  }

 protected:
  CanvasColorParams GetCanvas2DColorParams() const override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;
  void WillOverwriteCanvas() override;

 private:
  void FinalizeFrame() final;
  void FlushRecording();

  bool IsPaintable() const final;
  bool IsCanvas2DBufferValid() const override;

  void DrawTextInternal(const String&,
                        double,
                        double,
                        CanvasRenderingContext2DState::PaintType,
                        double* max_width = nullptr);
  const Font& AccessFont();

  scoped_refptr<CanvasResource> ProduceCanvasResource();

  String ColorSpaceAsString() const override;
  CanvasPixelFormat PixelFormat() const override;
  SkIRect dirty_rect_for_commit_;

  bool is_valid_size_ = false;

  std::mt19937 random_generator_;
  std::bernoulli_distribution bernoulli_distribution_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_
