/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/forward.h"  // IWYU pragma: keep (blink::Visitor)
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2050)
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"

// IWYU pragma: no_include "third_party/blink/renderer/platform/heap/visitor.h"

struct SkIRect;

namespace base {
struct PendingTask;
}  // namespace base

namespace cc {
class Layer;
}  // namespace cc

namespace blink {

class CanvasImageSource;
class ComputedStyle;
class Element;
class ExceptionState;
class ExecutionContext;
class ImageData;
class ImageDataSettings;
class MemoryManagedPaintCanvas;
class MemoryManagedPaintRecorder;
class Path2D;
class SVGResource;
enum class FlushReason;
enum class PredefinedColorSpace;

class MODULES_EXPORT CanvasRenderingContext2D final
    : public ScriptWrappable,
      public BaseRenderingContext2D,
      public SVGResourceClient,
      public CanvasHibernationHandler::Delegate {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override = default;

    CanvasRenderingContext* Create(
        ExecutionContext* execution_context,
        CanvasRenderingContextHost* host,
        const CanvasContextCreationAttributesCore& attrs) override;

    CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI()
        const override {
      return CanvasRenderingContext::CanvasRenderingAPI::k2D;
    }
  };

  CanvasRenderingContext2D(HTMLCanvasElement*,
                           const CanvasContextCreationAttributesCore&);
  ~CanvasRenderingContext2D() override;

  HTMLCanvasElement* canvas() const {
    DCHECK(!Host() || !Host()->IsOffscreenCanvas());
    return static_cast<HTMLCanvasElement*>(Host());
  }
  V8RenderingContext* AsV8RenderingContext() final;

  bool ShouldAntialias() const;
  void SetShouldAntialias(bool);

  void setFontForTesting(const String& new_font) override;

  void drawFocusIfNeeded(Element*);
  void drawFocusIfNeeded(Path2D*, Element*);

  void LoseContext(LostContextMode) override;

  // TaskObserver implementation
  void DidProcessTask(const base::PendingTask&) final;

  void StyleDidChange(const ComputedStyle* old_style,
                      const ComputedStyle& new_style) override;
  void LangAttributeChanged() override;

  // SVGResourceClient implementation
  void ResourceContentChanged(SVGResource*) override;

  void UpdateFilterReferences(const FilterOperations&);
  void ClearFilterReferences();

  // BaseRenderingContext2D implementation
  bool OriginClean() const final;
  void SetOriginTainted() final;
  void DisableAcceleration() override;
  bool ShouldDisableAccelerationBecauseOfReadback() const override;

  // CanvasHibernationHandler::Delegate implementation
  bool IsContextLost() const override { return isContextLost(); }
  bool IsPageVisible() const override {
    return canvas() && canvas()->IsPageVisible();
  }
  void ResetResourceProvider() override { ReplaceResourceProvider(nullptr); }
  void SetNeedsCompositingUpdate() override {
    if (canvas()) {
      canvas()->SetNeedsCompositingUpdate();
    }
  }
  void ClearCanvas2DLayerTexture() override {
    if (canvas()) {
      canvas()->ClearCanvas2DLayerTexture();
    }
  }

  // CanvasRenderingContext implementation
  bool IsComposited() const override;
  scoped_refptr<CanvasResource> PaintRenderingResultsToResource(
      SourceDrawingBuffer source_buffer,
      FlushReason reason) override;
  const std::optional<cc::PaintRecord>& GetLastRecordingForCanvas2D() override;

  int Width() const final;
  int Height() const final;

  bool CanCreateResourceProvider() final;

  RespectImageOrientationEnum RespectImageOrientation() const final;

  Color GetCurrentColor() const final;

  MemoryManagedPaintCanvas* GetOrCreatePaintCanvas() final;
  using BaseRenderingContext2D::GetPaintCanvas;  // Pull the non-const overload.
  const MemoryManagedPaintCanvas* GetPaintCanvas() const final;
  const MemoryManagedPaintRecorder* Recorder() const override;

  void WillDraw(const SkIRect& dirty_rect,
                CanvasPerformanceMonitor::DrawType) final;

  scoped_refptr<StaticBitmapImage> GetImage() final;

  sk_sp<PaintFilter> StateGetFilter() final;

  void PreFinalizeFrame() override;
  void FinalizeFrame(FlushReason) override;

  DOMMatrix* drawElement(Element* element,
                         double x,
                         double y,
                         ExceptionState& exception_state);
  DOMMatrix* drawElement(Element* element,
                         double x,
                         double y,
                         double dwidth,
                         double dheight,
                         ExceptionState& exception_state);

  DOMMatrix* drawElementImage(Element* element,
                              double x,
                              double y,
                              ExceptionState& exception_state);
  DOMMatrix* drawElementImage(Element* element,
                              double x,
                              double y,
                              double dwidth,
                              double dheight,
                              ExceptionState& exception_state);
  DOMMatrix* drawElementImage(Element* element,
                              double sx,
                              double sy,
                              double swidth,
                              double sheight,
                              double x,
                              double y,
                              ExceptionState& exception_state);
  DOMMatrix* drawElementImage(Element* element,
                              double sx,
                              double sy,
                              double swidth,
                              double sheight,
                              double x,
                              double y,
                              double dwidth,
                              double dheight,
                              ExceptionState& exception_state);

  CanvasRenderingContextHost* GetCanvasRenderingContextHost() const override;
  ExecutionContext* GetTopExecutionContext() const override;

  bool IsPaintable() const final;
  bool IsHibernating() const final;

  void WillDrawImage(CanvasImageSource*, bool image_is_texture_backed) final;

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason) override;

  void Trace(Visitor*) const override;

  ImageData* getImageDataInternal(int sx,
                                  int sy,
                                  int sw,
                                  int sh,
                                  ImageDataSettings*,
                                  ExceptionState&) final;

  void SendContextLostEventIfNeeded() override;

  CanvasResourceProvider* GetOrCreateResourceProvider() override;
  void SetCanvas2DResourceProviderForTesting(
      std::unique_ptr<CanvasResourceProvider> provider,
      const gfx::Size& size);

  // TODO(crbug.com/352263194): Migrate canvas_rendering_context_2d_test.cc
  // callsites and make this method private.
  CanvasHibernationHandler* GetHibernationHandler() const;

  CanvasResourceProvider* GetResourceProviderForTesting() const {
    return GetResourceProvider();
  }

  void EnableAccelerationIfPossible() override;

 protected:
  HTMLCanvasElement* HostAsHTMLCanvasElement() const final;
  UniqueFontSelector* GetFontSelector() const final;
  void SizeChanged() final;

  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  bool WillSetFont() const final;
  bool CurrentFontResolvedAndUpToDate() const final;
  bool ResolveFont(const String& new_font) final;

 private:
  friend class CanvasRenderingContext2DAutoRestoreSkCanvas;
  friend class CanvasRenderingContext2DTestBase;
  FRIEND_TEST_ALL_PREFIXES(CanvasRenderingContext2DTestAccelerated,
                           PrepareMailboxWhenContextIsLostWithFailedRestore);

  void ResetInternal() override;

  CanvasResourceProvider* GetResourceProvider() const override;
  void Dispose() override;

  std::unique_ptr<CanvasResourceProvider> CreateCanvasResourceProvider();

  DOMMatrix* DrawElementInternal(Element* element,
                                 std::optional<double> sx,
                                 std::optional<double> sy,
                                 std::optional<double> swidth,
                                 std::optional<double> sheight,
                                 double dx,
                                 double dy,
                                 std::optional<double> dwidth,
                                 std::optional<double> dheight,
                                 ExceptionState& exception_state);

  void PruneLocalFontCache(size_t target_size);

  void ScrollPathIntoViewInternal(const Path&);

  void DrawFocusIfNeededInternal(const Path&, Element*);
  bool FocusRingCallIsValid(const Path&, Element*);
  void DrawFocusRing(const Path&, Element*);
  void UpdateElementAccessibility(const Path&, Element*);

  bool HasAlpha() const override { return CreationAttributes().alpha; }
  bool IsDesynchronized() const override {
    return CreationAttributes().desynchronized;
  }
  void PageVisibilityChanged() override;
  void Stop() final;

  cc::Layer* CcLayer() const override;

  void ColorSchemeMayHaveChanged() override;

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>) override;

  // If the ResourceProvider currently exists, replaces it with a newly-created
  // CanvasResourceProvider.
  void DropAndRecreateExistingResourceProvider();

  // This method should be called only when `resource_provider_` is null.
  void RecreateResourceProvider();

  void WakeUpFromHibernation();

  FilterOperations filter_operations_;
  HashMap<String, FontDescription> fonts_resolved_using_current_style_;
  bool should_prune_local_font_cache_;
  LinkedHashSet<String> font_lru_list_;

  std::unique_ptr<CanvasHibernationHandler> hibernation_handler_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;

  // `did_fail_to_create_resource_provider_` prevents repeated attempts in
  // allocating resources after the first attempt failed.
  bool did_fail_to_create_resource_provider_ = false;

  // For privacy reasons we need to delay contextLost events until the page is
  // visible. In order to do this we will hold on to a bool here
  bool needs_context_lost_event_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_H_
