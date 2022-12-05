/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class Document;
class LayoutSVGRoot;
class LocalFrame;
class Node;
class Page;
class PaintController;
class SVGImageChromeClient;
class SVGImageForContainer;
class SVGSVGElement;
struct IntrinsicSizingInfo;

// SVGImage does not use Skia to draw images (as BitmapImage does) but instead
// handles drawing itself. Internally, SVGImage creates a detached & sandboxed
// Page containing an SVGDocument and reuses the existing paint code in Blink to
// draw the image. Because a single SVGImage can be referenced by multiple
// containers (see: SVGImageForContainer.h), each call to SVGImage::draw() may
// require (re-)laying out the inner SVGDocument.
//
// Using Page was an architectural hack and has surprising side-effects. Ideally
// SVGImage would use a lighter container around an SVGDocument that does not
// have the full Page machinery but still has the sandboxing security guarantees
// needed by SVGImage.
class CORE_EXPORT SVGImage final : public Image {
 public:
  static scoped_refptr<SVGImage> Create(ImageObserver* observer,
                                        bool is_multipart = false) {
    return base::AdoptRef(new SVGImage(observer, is_multipart));
  }

  static bool IsInSVGImage(const Node*);

  bool IsSVGImage() const override { return true; }
  gfx::Size SizeWithConfig(SizeConfig) const override;

  void CheckLoaded() const;
  bool CurrentFrameHasSingleSecurityOrigin() const override;

  void StartAnimation() override;
  void ResetAnimation() override;
  void RestoreAnimation();

  // Does the SVG image/document contain any animations?
  bool MaybeAnimated() override;

  // Advances an animated image. This will trigger an animation update for CSS
  // and advance the SMIL timeline by one frame.
  void AdvanceAnimationForTesting() override;
  SVGImageChromeClient& ChromeClientForTesting();

  static gfx::PointF OffsetForCurrentFrame(const gfx::RectF& dst_rect,
                                           const gfx::RectF& src_rect);

  // Service CSS and SMIL animations.
  void ServiceAnimations(base::TimeTicks monotonic_animation_start_time);

  void UpdateUseCounters(const Document&) const;

  // The defaultObjectSize is assumed to be unzoomed, i.e. it should
  // not have the effective zoom level applied. The returned size is
  // thus also independent of current zoom level.
  gfx::SizeF ConcreteObjectSize(const gfx::SizeF& default_object_size) const;

  // Get the intrinsic dimensions (width, height and aspect ratio) from this
  // SVGImage. Returns true if successful.
  bool GetIntrinsicSizingInfo(IntrinsicSizingInfo&) const;

  // Returns true if intrinsic dimensions can be extracted. (Essentially
  // returns true if GetIntrinsicSizingInfo would.)
  bool HasIntrinsicSizingInfo() const;

  PaintImage PaintImageForCurrentFrame() override;

  void SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme preferred_color_scheme);

 protected:
  // Whether or not size is available yet.
  bool IsSizeAvailable() override;

 private:
  // Accesses m_page.
  friend class SVGImageChromeClient;
  // Forwards calls to the various *ForContainer methods and other parts of
  // the the Image interface.
  friend class SVGImageForContainer;

  SVGImage(ImageObserver*, bool is_multipart);
  ~SVGImage() override;

  String FilenameExtension() const override;

  const AtomicString& MimeType() const override;

  SizeAvailability DataChanged(bool all_data_received) override;

  // FIXME: SVGImages are underreporting decoded sizes and will be unable
  // to prune because these functions are not implemented yet.
  void DestroyDecodedData() override {}

  // FIXME: Implement this to be less conservative.
  bool CurrentFrameKnownToBeOpaque() override { return false; }

  class DrawInfo {
    STACK_ALLOCATED();

   public:
    DrawInfo(const gfx::SizeF& container_size,
             float zoom,
             const KURL& url,
             bool is_dark_mode_enabled);

    gfx::SizeF CalculateResidualScale() const;
    float Zoom() const { return zoom_; }
    const gfx::SizeF& ContainerSize() const { return container_size_; }
    const gfx::Size& RoundedContainerSize() const {
      return rounded_container_size_;
    }
    const KURL& Url() const { return url_; }
    bool IsDarkModeEnabled() const { return is_dark_mode_enabled_; }

   private:
    const gfx::SizeF container_size_;
    const gfx::Size rounded_container_size_;
    const float zoom_;
    const KURL& url_;
    const bool is_dark_mode_enabled_;
  };

  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const gfx::RectF& dst_rect,
            const gfx::RectF& src_rect,
            const ImageDrawOptions&) override;
  void DrawForContainer(const DrawInfo&,
                        cc::PaintCanvas*,
                        const cc::PaintFlags&,
                        const gfx::RectF& dst_rect,
                        const gfx::RectF& src_rect);
  void DrawPatternForContainer(const DrawInfo&,
                               GraphicsContext&,
                               const cc::PaintFlags&,
                               const gfx::RectF& dst_rect,
                               const ImageTilingInfo&);
  void PopulatePaintRecordForCurrentFrameForContainer(const DrawInfo&,
                                                      PaintImageBuilder&);

  // Paints the current frame. Returns new PaintRecord.
  sk_sp<PaintRecord> PaintRecordForCurrentFrame(const DrawInfo&);

  void DrawInternal(const DrawInfo&,
                    cc::PaintCanvas*,
                    const cc::PaintFlags&,
                    const gfx::RectF& dst_rect,
                    const gfx::RectF& unzoomed_src_rect);
  bool ApplyShader(cc::PaintFlags&,
                   const SkMatrix& local_matrix,
                   const gfx::RectF& src_rect,
                   const ImageDrawOptions&) override;
  bool ApplyShaderForContainer(const DrawInfo&,
                               cc::PaintFlags&,
                               const SkMatrix& local_matrix);
  bool ApplyShaderInternal(const DrawInfo&,
                           cc::PaintFlags&,
                           const SkMatrix& local_matrix);

  void StopAnimation();
  void ScheduleTimelineRewind();
  void FlushPendingTimelineRewind();

  Page* GetPageForTesting() { return page_; }
  void LoadCompleted();
  void NotifyAsyncLoadCompleted();

  LocalFrame* GetFrame() const;
  SVGSVGElement* RootElement() const;
  LayoutSVGRoot* LayoutRoot() const;

  class SVGImageLocalFrameClient;

  Persistent<SVGImageChromeClient> chrome_client_;
  Persistent<Page> page_;
  std::unique_ptr<PaintController> paint_controller_;
  Persistent<AgentGroupScheduler> agent_group_scheduler_;

  // When an SVG image has no intrinsic size, the size depends on the default
  // object size, which in turn depends on the container. One SVGImage may
  // belong to multiple containers so the final image size can't be known in
  // SVGImage. SVGImageForContainer carries the final image size, also called
  // the "concrete object size". For more, see: SVGImageForContainer.h
  LayoutSize intrinsic_size_;
  bool has_pending_timeline_rewind_;

  enum LoadState {
    kDataChangedNotStarted,
    kInDataChanged,
    kWaitingForAsyncLoadCompletion,
    kLoadCompleted,
  };

  LoadState load_state_ = kDataChangedNotStarted;

  Persistent<SVGImageLocalFrameClient> frame_client_;
  FRIEND_TEST_ALL_PREFIXES(ElementFragmentAnchorTest,
                           SVGDocumentDoesntCreateFragment);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, SupportsSubsequenceCaching);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, LayoutShiftTrackerDisabled);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, SetSizeOnVisualViewport);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, IsSizeAvailable);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, DisablesSMILEvents);
};

template <>
struct DowncastTraits<SVGImage> {
  static bool AllowFrom(const Image& image) { return image.IsSVGImage(); }
};

class ImageObserverDisabler {
  STACK_ALLOCATED();

 public:
  ImageObserverDisabler(Image* image) : image_(image) {
    image_->SetImageObserverDisabled(true);
  }

  ImageObserverDisabler(const ImageObserverDisabler&) = delete;
  ImageObserverDisabler& operator=(const ImageObserverDisabler&) = delete;

  ~ImageObserverDisabler() { image_->SetImageObserverDisabled(false); }

 private:
  Image* image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_H_
