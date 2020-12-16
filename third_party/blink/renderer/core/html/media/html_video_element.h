/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_VIDEO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_VIDEO_ELEMENT_H_

#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {
class ImageBitmapOptions;
class IntersectionObserverEntry;
class MediaCustomControlsFullscreenDetector;
class MediaRemotingInterstitial;
class PictureInPictureInterstitial;
class VideoWakeLock;

class CORE_EXPORT HTMLVideoElement final
    : public HTMLMediaElement,
      public CanvasImageSource,
      public ImageBitmapSource,
      public Supplementable<HTMLVideoElement> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const int kNoAlreadyUploadedFrame = -1;

  explicit HTMLVideoElement(Document&);
  void Trace(Visitor*) const override;

  bool HasPendingActivity() const final;

  // Node override.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  unsigned videoWidth() const;
  unsigned videoHeight() const;

  IntSize videoVisibleSize() const;

  bool IsDefaultIntrinsicSize() const {
    return is_default_overridden_intrinsic_size_;
  }

  // Fullscreen
  void webkitEnterFullscreen();
  void webkitExitFullscreen();
  bool webkitSupportsFullscreen();
  bool webkitDisplayingFullscreen();
  bool UsesOverlayFullscreenVideo() const override;
  void DidEnterFullscreen();
  void DidExitFullscreen();

  // Statistics
  unsigned webkitDecodedFrameCount() const;
  unsigned webkitDroppedFrameCount() const;

  // Used by canvas to gain raw pixel access
  //
  // PaintFlags is optional. If unspecified, its blend mode defaults to kSrc.
  void PaintCurrentFrame(
      cc::PaintCanvas*,
      const IntRect&,
      const cc::PaintFlags*,
      int already_uploaded_id = kNoAlreadyUploadedFrame,
      WebMediaPlayer::VideoFrameUploadMetadata* out_metadata = nullptr) const;

  // Used by WebGL to do GPU-GPU texture copy if possible.
  bool CopyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface*,
      GLenum target,
      GLuint texture,
      GLenum internal_format,
      GLenum format,
      GLenum type,
      GLint level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      WebMediaPlayer::VideoFrameUploadMetadata* out_metadata);

  // Used by WebGL to do YUV-RGB, CPU-GPU texture copy if possible.
  bool CopyVideoYUVDataToPlatformTexture(
      gpu::gles2::GLES2Interface*,
      GLenum target,
      GLuint texture,
      GLenum internal_format,
      GLenum format,
      GLenum type,
      GLint level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      WebMediaPlayer::VideoFrameUploadMetadata* out_metadata);

  // Used by WebGL to do CPU-GPU texture upload if possible.
  bool TexImageImpl(WebMediaPlayer::TexImageFunctionID,
                    GLenum target,
                    gpu::gles2::GLES2Interface*,
                    GLuint texture,
                    GLint level,
                    GLint internalformat,
                    GLenum format,
                    GLenum type,
                    GLint xoffset,
                    GLint yoffset,
                    GLint zoffset,
                    bool flip_y,
                    bool premultiply_alpha);

  // Used by WebGL to do GPU_GPU texture sharing if possible.
  bool PrepareVideoFrameForWebGL(
      gpu::gles2::GLES2Interface*,
      GLenum target,
      GLuint texture,
      int already_uploaded_id,
      WebMediaPlayer::VideoFrameUploadMetadata* out_metadata);

  bool HasAvailableVideoFrame() const;

  KURL PosterImageURL() const override;

  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               const FloatSize&) override;
  bool IsVideoElement() const override { return true; }
  bool WouldTaintOrigin() const override;
  FloatSize ElementSize(const FloatSize&,
                        const RespectImageOrientationEnum) const override;
  const KURL& SourceURL() const override { return currentSrc(); }
  bool IsHTMLVideoElement() const override { return true; }
  // Video elements currently always go through RAM when used as a canvas image
  // source.
  bool IsAccelerated() const override { return false; }

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  // WebMediaPlayerClient implementation.
  void OnBecamePersistentVideo(bool) final;
  void OnRequestVideoFrameCallback() final;

  bool IsPersistent() const;

  bool IsRemotingInterstitialVisible() const;

  void MediaRemotingStarted(const WebString& remote_device_friendly_name) final;
  bool SupportsPictureInPicture() const final;
  void MediaRemotingStopped(int error_code) final;
  DisplayType GetDisplayType() const final;
  bool IsInAutoPIP() const final;
  void OnPictureInPictureStateChange() final;

  // Used by the PictureInPictureController as callback when the video element
  // enters or exits Picture-in-Picture state.
  void OnEnteredPictureInPicture();
  void OnExitedPictureInPicture();

  void SetIsEffectivelyFullscreen(blink::WebFullscreenVideoStatus);
  void SetIsDominantVisibleContent(bool is_dominant);

  VideoWakeLock* wake_lock_for_tests() const { return wake_lock_; }

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  void OnWebMediaPlayerCreated() final;
  void OnWebMediaPlayerCleared() final;

  void AttributeChanged(const AttributeModificationParams& params) override;

 private:
  friend class MediaCustomControlsFullscreenDetectorTest;
  friend class HTMLMediaElementEventListenersTest;
  friend class HTMLVideoElementPersistentTest;
  friend class VideoFillingViewportTest;

  // ExecutionContextLifecycleStateObserver functions.
  void ContextDestroyed() final;

  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void AttachLayoutTree(AttachContext&) override;
  void UpdatePosterImage();
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  bool IsURLAttribute(const Attribute&) const override;
  const AtomicString ImageSourceURL() const override;

  void OnPlay() final;
  void OnLoadStarted() final;
  void OnLoadFinished() final;

  // Video-specific overrides for part of the media::mojom::MediaPlayer
  // interface, fully implemented in the parent class HTMLMediaElement.
  void RequestEnterPictureInPicture() final;
  void RequestExitPictureInPicture() final;

  void DidMoveToNewDocument(Document& old_document) override;

  void UpdatePictureInPictureAvailability();

  void OnIntersectionChangedForLazyLoad(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);

  Member<HTMLImageLoader> image_loader_;
  Member<MediaCustomControlsFullscreenDetector>
      custom_controls_fullscreen_detector_;
  Member<VideoWakeLock> wake_lock_;

  Member<MediaRemotingInterstitial> remoting_interstitial_;
  Member<PictureInPictureInterstitial> picture_in_picture_interstitial_;

  AtomicString default_poster_url_;

  // Represents whether the video is 'persistent'. It is used for videos with
  // custom controls that are in auto-pip (Android). This boolean is used by a
  // CSS rule.
  bool is_persistent_ : 1;

  // Whether the video is currently in auto-pip (Android). It is not similar to
  // a video being in regular Picture-in-Picture mode.
  bool is_auto_picture_in_picture_ : 1;

  // Whether this element is in overlay fullscreen mode.
  bool in_overlay_fullscreen_video_ : 1;

  // Whether the video element should be considered as fullscreen with regards
  // to display type and other UI features. This does not mean the DOM element
  // is fullscreen.
  bool is_effectively_fullscreen_ : 1;

  bool is_default_overridden_intrinsic_size_ : 1;

  bool video_has_played_ : 1;

  // True, if the video element occupies most of the viewport.
  bool mostly_filling_viewport_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_VIDEO_ELEMENT_H_
