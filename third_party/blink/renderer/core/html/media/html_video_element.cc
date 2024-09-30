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

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "cc/paint/paint_canvas.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"
#include "third_party/blink/renderer/core/html/media/media_remoting_interstitial.h"
#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"
#include "third_party/blink/renderer/core/html/media/picture_in_picture_interstitial.h"
#include "third_party/blink/renderer/core/html/media/video_frame_callback_requester.h"
#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/video_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {
// Represents the visibility threshold to be used by the
// |visibility_tracker_|. Where visibility is defined as: intersecting
// with the viewport and not occluded by other html elements within the page,
// with the exception of MediaControls.
//
// An HTMLVideoElement with visibility greater or equal than a given area
// measured in square CSS pixels (`kVisibilityThreshold`) is considered visible,
// and not visible otherwise.
constexpr int kVisibilityThreshold = 10000;

constexpr base::TimeDelta kTemporaryResourceDeletionDelay = base::Seconds(3);
}  // namespace

HTMLVideoElement::HTMLVideoElement(Document& document)
    : HTMLMediaElement(html_names::kVideoTag, document),
      remoting_interstitial_(nullptr),
      picture_in_picture_interstitial_(nullptr),
      is_persistent_(false),
      is_auto_picture_in_picture_(false),
      is_effectively_fullscreen_(false),
      video_has_played_(false),
      mostly_filling_viewport_(false),
      cache_deleting_timer_(
          GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &HTMLVideoElement::ResetCache) {
  if (document.GetSettings()) {
    default_poster_url_ =
        AtomicString(document.GetSettings()->GetDefaultVideoPosterURL());
  }

  custom_controls_fullscreen_detector_ =
      MakeGarbageCollected<MediaCustomControlsFullscreenDetector>(*this);

  wake_lock_ = MakeGarbageCollected<VideoWakeLock>(*this);

  EnsureUserAgentShadowRoot();
  UpdateStateIfNeeded();
}

void HTMLVideoElement::Trace(Visitor* visitor) const {
  visitor->Trace(image_loader_);
  visitor->Trace(custom_controls_fullscreen_detector_);
  visitor->Trace(visibility_tracker_);
  visitor->Trace(wake_lock_);
  visitor->Trace(remoting_interstitial_);
  visitor->Trace(picture_in_picture_interstitial_);
  visitor->Trace(cache_deleting_timer_);
  Supplementable<HTMLVideoElement>::Trace(visitor);
  HTMLMediaElement::Trace(visitor);
}

bool HTMLVideoElement::HasPendingActivity() const {
  return HTMLMediaElement::HasPendingActivity() ||
         (image_loader_ && image_loader_->HasPendingActivity());
}

Node::InsertionNotificationRequest HTMLVideoElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected())
    custom_controls_fullscreen_detector_->Attach();

  auto insertion_notification_request =
      HTMLMediaElement::InsertedInto(insertion_point);

  UpdateVideoVisibilityTracker();

  return insertion_notification_request;
}

void HTMLVideoElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLMediaElement::RemovedFrom(insertion_point);
  custom_controls_fullscreen_detector_->Detach();
  UpdateVideoVisibilityTracker();
  SetPersistentState(false);
}

void HTMLVideoElement::ContextDestroyed() {
  custom_controls_fullscreen_detector_->ContextDestroyed();
  UpdateVideoVisibilityTracker();
  HTMLMediaElement::ContextDestroyed();
}

bool HTMLVideoElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLVideoElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutVideo>(this);
}

void HTMLVideoElement::AttachLayoutTree(AttachContext& context) {
  HTMLMediaElement::AttachLayoutTree(context);
  // Initiate loading of the poster image if a default poster image is
  // specified and no poster has been loaded (=> no ImageLoader created).
  if (!default_poster_url_.empty() && !image_loader_) {
    UpdatePosterImage();
  }
  if (image_loader_ && GetLayoutObject()) {
    image_loader_->OnAttachLayoutTree();
  }
}

void HTMLVideoElement::UpdatePosterImage() {
  if (!image_loader_) {
    image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
  }
  image_loader_->UpdateFromElement();
}

void HTMLVideoElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
    const AtomicString& height = FastGetAttribute(html_names::kHeightAttr);
    if (height)
      ApplyAspectRatioToStyle(value, height, style);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
    const AtomicString& width = FastGetAttribute(html_names::kWidthAttr);
    if (width)
      ApplyAspectRatioToStyle(width, value, style);
  } else {
    HTMLMediaElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

bool HTMLVideoElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr)
    return true;
  return HTMLMediaElement::IsPresentationAttribute(name);
}

void HTMLVideoElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kPosterAttr) {
    const KURL poster_image_url = PosterImageURL();
    // Load the poster if set, |VideoPainter| will decide whether to draw
    // it. Only create an ImageLoader if a non-empty URL is seen.
    if (image_loader_ || !poster_image_url.IsEmpty()) {
      UpdatePosterImage();
    }
    // Notify the player when the poster image URL changes.
    if (GetWebMediaPlayer()) {
      GetWebMediaPlayer()->SetPoster(poster_image_url);
    }
    // Media remoting and picture in picture doesn't show the original poster
    // image, instead, it shows a grayscaled and blurred copy.
    if (remoting_interstitial_)
      remoting_interstitial_->OnPosterImageChanged();
    if (picture_in_picture_interstitial_)
      picture_in_picture_interstitial_->OnPosterImageChanged();
  } else {
    HTMLMediaElement::ParseAttribute(params);
  }
}

unsigned HTMLVideoElement::videoWidth() const {
  if (!GetWebMediaPlayer())
    return 0;
  return GetWebMediaPlayer()->NaturalSize().width();
}

unsigned HTMLVideoElement::videoHeight() const {
  if (!GetWebMediaPlayer())
    return 0;
  return GetWebMediaPlayer()->NaturalSize().height();
}

gfx::Size HTMLVideoElement::videoVisibleSize() const {
  return GetWebMediaPlayer() ? GetWebMediaPlayer()->VisibleSize() : gfx::Size();
}

bool HTMLVideoElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kPosterAttr ||
         HTMLMediaElement::IsURLAttribute(attribute);
}

const AtomicString HTMLVideoElement::ImageSourceURL() const {
  const AtomicString& url = FastGetAttribute(html_names::kPosterAttr);
  if (!StripLeadingAndTrailingHTMLSpaces(url).empty())
    return url;
  return default_poster_url_;
}

void HTMLVideoElement::UpdatePictureInPictureAvailability() {
  if (!web_media_player_)
    return;

  for (auto& observer : GetMediaPlayerObserverRemoteSet())
    observer->OnPictureInPictureAvailabilityChanged(SupportsPictureInPicture());
}

// TODO(zqzhang): this callback could be used to hide native controls instead of
// using a settings. See `HTMLMediaElement::onMediaControlsEnabledChange`.
void HTMLVideoElement::SetPersistentState(bool persistent) {
  SetPersistentStateInternal(persistent);
  if (GetWebMediaPlayer())
    GetWebMediaPlayer()->SetPersistentState(persistent);
}

void HTMLVideoElement::SetPersistentStateInternal(bool persistent) {
  is_auto_picture_in_picture_ = persistent;

  if (persistent) {
    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(GetDocument());
    // Only set the video in persistent mode if it is not using native controls
    // and is currently fullscreen.
    if (!fullscreen_element || IsFullscreen())
      return;

    is_persistent_ = true;
    PseudoStateChanged(CSSSelector::kPseudoVideoPersistent);

    // The video is also marked as containing a persistent video to simplify the
    // internal CSS logic.
    for (Element* element = this; element && element != fullscreen_element;
         element = element->ParentOrShadowHostElement()) {
      element->SetContainsPersistentVideo(true);
    }
    fullscreen_element->SetContainsPersistentVideo(true);
  } else {
    if (!is_persistent_)
      return;

    is_persistent_ = false;
    PseudoStateChanged(CSSSelector::kPseudoVideoPersistent);

    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(GetDocument());
    // If the page is no longer fullscreen, the full tree will have to be
    // traversed to make sure things are cleaned up.
    for (Element* element = this; element && element != fullscreen_element;
         element = element->ParentOrShadowHostElement()) {
      element->SetContainsPersistentVideo(false);
    }
    if (fullscreen_element)
      fullscreen_element->SetContainsPersistentVideo(false);
  }

  if (GetWebMediaPlayer())
    GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
}

void HTMLVideoElement::CreateVisibilityTrackerIfNeeded() {
  if (!RuntimeEnabledFeatures::AutoPictureInPictureVideoHeuristicsEnabled()) {
    return;
  }

  if (visibility_tracker_) {
    return;
  }

  // Callback used by |MediaVideoVisibilityTracker| to report whether |this|
  // meets/does not meet the visibility threshold (kVisibilityThreshold).
  auto report_visibility_cb = WTF::BindRepeating(
      &HTMLVideoElement::ReportVisibility, WrapWeakPersistent(this));

  visibility_tracker_ = MakeGarbageCollected<MediaVideoVisibilityTracker>(
      *this, kVisibilityThreshold, std::move(report_visibility_cb));
}

void HTMLVideoElement::ReportVisibility(bool meets_visibility_threshold) {
  if (GetWebMediaPlayer()) {
    for (auto& observer : GetMediaPlayerObserverRemoteSet()) {
      observer->OnVideoVisibilityChanged(meets_visibility_threshold);
    }
  }
}

void HTMLVideoElement::ResetCache(TimerBase*) {
  resource_provider_.reset();
}

bool HTMLVideoElement::IsPersistent() const {
  return is_persistent_;
}

void HTMLVideoElement::OnPlay() {
  if (!video_has_played_) {
    video_has_played_ = true;
    UpdatePictureInPictureAvailability();
  }

  CreateVisibilityTrackerIfNeeded();
  UpdateVideoVisibilityTracker();

  if (!RuntimeEnabledFeatures::VideoAutoFullscreenEnabled() ||
      FastHasAttribute(html_names::kPlaysinlineAttr)) {
    return;
  }

  webkitEnterFullscreen();
}

void HTMLVideoElement::OnLoadStarted() {
  web_media_player_->BecameDominantVisibleContent(mostly_filling_viewport_);
}

void HTMLVideoElement::OnLoadFinished() {
  // If the player did a lazy load, it's expecting to be called when the
  // element actually becomes visible to complete the load.
  if (web_media_player_->DidLazyLoad() && !PotentiallyPlaying()) {
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        GetDocument(),
        WTF::BindRepeating(&HTMLVideoElement::OnIntersectionChangedForLazyLoad,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kMediaIntersectionObserver,
        IntersectionObserver::Params{
            .thresholds = {IntersectionObserver::kMinimumThreshold}});
    lazy_load_intersection_observer_->observe(this);
  }

  UpdatePictureInPictureAvailability();
}

void HTMLVideoElement::UpdateVideoVisibilityTracker() {
  if (!visibility_tracker_) {
    return;
  }

  visibility_tracker_->UpdateVisibilityTrackerState();
}

void HTMLVideoElement::RequestEnterPictureInPicture() {
  PictureInPictureController::From(GetDocument())
      .EnterPictureInPicture(this, /*promise=*/nullptr);
}

void HTMLVideoElement::RequestMediaRemoting() {
  GetWebMediaPlayer()->RequestMediaRemoting();
}

void HTMLVideoElement::RequestVisibility(
    RequestVisibilityCallback request_visibility_cb) {
  if (!visibility_tracker_) {
    std::move(request_visibility_cb).Run(false);
    return;
  }

  visibility_tracker_->RequestVisibility(std::move(request_visibility_cb));
}

void HTMLVideoElement::PaintCurrentFrame(cc::PaintCanvas* canvas,
                                         const gfx::Rect& dest_rect,
                                         const cc::PaintFlags* flags) const {
  if (!GetWebMediaPlayer())
    return;

  cc::PaintFlags media_flags;
  if (flags) {
    media_flags = *flags;
  } else {
    media_flags.setAlphaf(1.0f);
    media_flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
    media_flags.setBlendMode(SkBlendMode::kSrc);
  }

  GetWebMediaPlayer()->Paint(canvas, dest_rect, media_flags);
}

bool HTMLVideoElement::HasAvailableVideoFrame() const {
  if (auto* wmp = GetWebMediaPlayer())
    return wmp->HasAvailableVideoFrame();
  return false;
}

bool HTMLVideoElement::HasReadableVideoFrame() const {
  if (auto* wmp = GetWebMediaPlayer()) {
    return wmp->HasReadableVideoFrame();
  }
  return false;
}

void HTMLVideoElement::OnFirstFrame(base::TimeTicks frame_time,
                                    size_t bytes_to_first_frame) {
  DCHECK(GetWebMediaPlayer());
  LayoutObject* layout_object = GetLayoutObject();
  // HasLocalBorderBoxProperties will be false in some cases, specifically
  // picture-in-picture video may return false here.
  if (layout_object &&
      layout_object->FirstFragment().HasLocalBorderBoxProperties()) {
    VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
    video_timing->SetFirstVideoFrameTime(frame_time);
    video_timing->SetIsSufficientContentLoadedForPaint();
    video_timing->SetUrl(currentSrc());
    video_timing->SetContentSizeForEntropy(bytes_to_first_frame);
    video_timing->SetTimingAllowPassed(
        GetWebMediaPlayer()->PassedTimingAllowOriginCheck());

    PaintTimingDetector::NotifyImagePaint(
        *layout_object, videoVisibleSize(), *video_timing,
        layout_object->FirstFragment().LocalBorderBoxProperties(),
        layout_object->AbsoluteBoundingBoxRect());
  }
}

void HTMLVideoElement::webkitEnterFullscreen() {
  if (!IsFullscreen()) {
    FullscreenOptions* options = FullscreenOptions::Create();
    options->setNavigationUI("hide");
    Fullscreen::RequestFullscreen(*this, options,
                                  FullscreenRequestType::kPrefixed);
  }
}

void HTMLVideoElement::webkitExitFullscreen() {
  if (IsFullscreen())
    Fullscreen::ExitFullscreen(GetDocument());
}

bool HTMLVideoElement::webkitSupportsFullscreen() {
  return Fullscreen::FullscreenEnabled(GetDocument());
}

bool HTMLVideoElement::webkitDisplayingFullscreen() {
  return IsFullscreen();
}

void HTMLVideoElement::DidEnterFullscreen() {
  UpdateControlsVisibility();

  if (GetDisplayType() == DisplayType::kPictureInPicture && !IsInAutoPIP()) {
    PictureInPictureController::From(GetDocument())
        .ExitPictureInPicture(this, nullptr);
  }

  if (GetWebMediaPlayer()) {
    // FIXME: There is no embedder-side handling in web test mode.
    if (!WebTestSupport::IsRunningWebTest())
      GetWebMediaPlayer()->EnteredFullscreen();
    GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
  }
}

void HTMLVideoElement::DidExitFullscreen() {
  UpdateControlsVisibility();

  if (GetWebMediaPlayer()) {
    GetWebMediaPlayer()->ExitedFullscreen();
    GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
  }

  if (RuntimeEnabledFeatures::VideoAutoFullscreenEnabled() &&
      !FastHasAttribute(html_names::kPlaysinlineAttr)) {
    pause();
  }
}

void HTMLVideoElement::DidMoveToNewDocument(Document& old_document) {
  if (image_loader_)
    image_loader_->ElementDidMoveToNewDocument();

  wake_lock_->ElementDidMoveToNewDocument();

  if (visibility_tracker_) {
    // Ensure that the |visibility_tracker_| is detached when |this| is moved to
    // a new document. Calling |ElementDidMoveToNewDocument| on the tracker at
    // this point prevents having the tracker attached to an old document. The
    // subsequent call to |UpdateVideoVisibilityTracker| will re-attach
    // the tracker to the new document if needed.
    visibility_tracker_->ElementDidMoveToNewDocument();
    UpdateVideoVisibilityTracker();
  }

  HTMLMediaElement::DidMoveToNewDocument(old_document);
  if (image_loader_) {
    image_loader_->UpdateFromElement();
  }
}

unsigned HTMLVideoElement::webkitDecodedFrameCount() const {
  if (!GetWebMediaPlayer())
    return 0;

  return GetWebMediaPlayer()->DecodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const {
  if (!GetWebMediaPlayer())
    return 0;

  return GetWebMediaPlayer()->DroppedFrameCount();
}

KURL HTMLVideoElement::PosterImageURL() const {
  String url = StripLeadingAndTrailingHTMLSpaces(ImageSourceURL());
  if (url.empty())
    return KURL();
  return GetDocument().CompleteURL(url);
}

bool HTMLVideoElement::IsDefaultPosterImageURL() const {
  return ImageSourceURL() == default_poster_url_;
}

scoped_refptr<StaticBitmapImage> HTMLVideoElement::CreateStaticBitmapImage(
    bool allow_accelerated_images,
    std::optional<gfx::Size> size,
    bool reinterpret_as_srgb) {
  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  scoped_refptr<media::VideoFrame> media_video_frame;
  if (auto* wmp = GetWebMediaPlayer()) {
    media_video_frame = wmp->GetCurrentFrameThenUpdate();
    video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!media_video_frame || !video_renderer)
    return nullptr;

  gfx::Size dest_size = size.value_or(media_video_frame->natural_size());
  if (dest_size.width() <= 0 || dest_size.height() <= 0) {
    return nullptr;
  }

  // TODO(https://crbug.com/1341235): The choice of color type and alpha type
  // is inappropriate in many circumstances.
  const auto resource_provider_info = SkImageInfo::Make(
      gfx::SizeToSkISize(dest_size), kN32_SkColorType, kPremul_SkAlphaType,
      reinterpret_as_srgb
          ? SkColorSpace::MakeSRGB()
          : media_video_frame->CompatRGBColorSpace().ToSkColorSpace());
  if (!resource_provider_ ||
      (resource_provider_->IsAccelerated() &&
       resource_provider_->IsGpuContextLost()) ||
      allow_accelerated_images != allow_accelerated_images_ ||
      resource_provider_info != resource_provider_info_) {
    viz::RasterContextProvider* raster_context_provider = nullptr;
    if (allow_accelerated_images) {
      if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
        if (auto* context_provider = wrapper->ContextProvider())
          raster_context_provider = context_provider->RasterContextProvider();
      }
    }
    resource_provider_.reset();
    // Providing a null |raster_context_provider| creates a software provider.
    resource_provider_ = CreateResourceProviderForVideoFrame(
        resource_provider_info, raster_context_provider);
    if (!resource_provider_)
      return nullptr;
    resource_provider_info_ = resource_provider_info;
    allow_accelerated_images_ = allow_accelerated_images;
  }
  cache_deleting_timer_.StartOneShot(kTemporaryResourceDeletionDelay,
                                     FROM_HERE);

  auto image = CreateImageFromVideoFrame(
      std::move(media_video_frame),
      /*allow_zero_copy_images=*/true, resource_provider_.get(), video_renderer,
      gfx::Rect(dest_size),
      /*prefer_tagged_orientation=*/true, reinterpret_as_srgb);
  if (image)
    image->SetOriginClean(!WouldTaintOrigin());
  return image;
}

scoped_refptr<Image> HTMLVideoElement::GetSourceImageForCanvas(
    FlushReason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition) {
  // UnpremultiplyAlpha is not implemented yet.
  DCHECK_EQ(alpha_disposition, kPremultiplyAlpha);

  scoped_refptr<Image> snapshot = CreateStaticBitmapImage();
  if (!snapshot) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return snapshot;
}

bool HTMLVideoElement::WouldTaintOrigin() const {
  return !IsMediaDataCorsSameOrigin();
}

gfx::SizeF HTMLVideoElement::ElementSize(
    const gfx::SizeF&,
    const RespectImageOrientationEnum) const {
  return gfx::SizeF(videoWidth(), videoHeight());
}

gfx::Size HTMLVideoElement::BitmapSourceSize() const {
  return gfx::Size(videoWidth(), videoHeight());
}

ScriptPromise<ImageBitmap> HTMLVideoElement::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (getNetworkState() == HTMLMediaElement::kNetworkEmpty) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The provided element has not retrieved data.");
    return EmptyPromise();
  }
  if (!HasAvailableVideoFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The provided element's player has no current data.");
    return EmptyPromise();
  }

  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      options, exception_state);
}

void HTMLVideoElement::MediaRemotingStarted(
    const WebString& remote_device_friendly_name) {
  is_remote_rendering_ = true;
  remote_device_friendly_name_ = remote_device_friendly_name;
  OnRemotePlaybackMetadataChange();
  if (!remoting_interstitial_) {
    remoting_interstitial_ =
        MakeGarbageCollected<MediaRemotingInterstitial>(*this);
    ShadowRoot& shadow_root = EnsureUserAgentShadowRoot();
    shadow_root.InsertBefore(remoting_interstitial_, shadow_root.firstChild());
    HTMLMediaElement::AssertShadowRootChildren(shadow_root);
  }
  remoting_interstitial_->Show(remote_device_friendly_name);
}

void HTMLVideoElement::MediaRemotingStopped(int error_code) {
  is_remote_rendering_ = false;
  remote_device_friendly_name_.Reset();
  OnRemotePlaybackMetadataChange();
  if (remoting_interstitial_)
    remoting_interstitial_->Hide(error_code);
}

bool HTMLVideoElement::SupportsPictureInPicture() const {
  return PictureInPictureController::From(GetDocument())
             .IsElementAllowed(*this) ==
         PictureInPictureController::Status::kEnabled;
}

DisplayType HTMLVideoElement::GetDisplayType() const {
  if (is_auto_picture_in_picture_ ||
      PictureInPictureController::IsElementInPictureInPicture(this)) {
    return DisplayType::kPictureInPicture;
  }

  if (is_effectively_fullscreen_)
    return DisplayType::kFullscreen;

  return HTMLMediaElement::GetDisplayType();
}

bool HTMLVideoElement::IsInAutoPIP() const {
  return is_auto_picture_in_picture_;
}

void HTMLVideoElement::OnPictureInPictureStateChange() {
  if (GetDisplayType() != DisplayType::kPictureInPicture || IsInAutoPIP()) {
    return;
  }

  PictureInPictureController::From(GetDocument())
      .OnPictureInPictureStateChange();
}

void HTMLVideoElement::OnEnteredPictureInPicture() {
  if (!picture_in_picture_interstitial_) {
    picture_in_picture_interstitial_ =
        MakeGarbageCollected<PictureInPictureInterstitial>(*this);
    ShadowRoot& shadow_root = EnsureUserAgentShadowRoot();
    shadow_root.InsertBefore(picture_in_picture_interstitial_,
                             shadow_root.firstChild());
    HTMLMediaElement::AssertShadowRootChildren(shadow_root);
  }
  picture_in_picture_interstitial_->Show();

  PseudoStateChanged(CSSSelector::kPseudoPictureInPicture);

  DCHECK(GetWebMediaPlayer());
  GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
}

void HTMLVideoElement::OnExitedPictureInPicture() {
  if (picture_in_picture_interstitial_)
    picture_in_picture_interstitial_->Hide();

  PseudoStateChanged(CSSSelector::kPseudoPictureInPicture);

  if (GetWebMediaPlayer())
    GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
}

void HTMLVideoElement::SetIsEffectivelyFullscreen(
    blink::WebFullscreenVideoStatus status) {
  is_effectively_fullscreen_ =
      status != blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen;
  if (GetWebMediaPlayer()) {
    for (auto& observer : GetMediaPlayerObserverRemoteSet())
      observer->OnMediaEffectivelyFullscreenChanged(status);

    GetWebMediaPlayer()->SetIsEffectivelyFullscreen(status);
    GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
  }
}

void HTMLVideoElement::SetIsDominantVisibleContent(bool is_dominant) {
  if (mostly_filling_viewport_ != is_dominant) {
    mostly_filling_viewport_ = is_dominant;
    auto* player = GetWebMediaPlayer();
    if (player)
      player->BecameDominantVisibleContent(mostly_filling_viewport_);

    auto* local_frame_view = GetDocument().View();
    if (local_frame_view)
      local_frame_view->NotifyVideoIsDominantVisibleStatus(this, is_dominant);
  }
}

void HTMLVideoElement::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (event_type == event_type_names::kEnterpictureinpicture) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEnterPictureInPictureEventListener);
  } else if (event_type == event_type_names::kLeavepictureinpicture) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kLeavePictureInPictureEventListener);
  }

  HTMLMediaElement::AddedEventListener(event_type, registered_listener);
}

bool HTMLVideoElement::IsRemotingInterstitialVisible() const {
  return remoting_interstitial_ && remoting_interstitial_->IsVisible();
}

void HTMLVideoElement::OnIntersectionChangedForLazyLoad(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = (entries.back()->intersectionRatio() > 0);
  if (!is_visible || !web_media_player_)
    return;

  lazy_load_intersection_observer_->disconnect();
  lazy_load_intersection_observer_ = nullptr;

  auto notify_visible = [](HTMLVideoElement* self) {
    if (self && self->web_media_player_)
      self->web_media_player_->OnBecameVisible();
  };

  GetDocument()
      .GetTaskRunner(TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(notify_visible, WrapWeakPersistent(this)));
}

void HTMLVideoElement::OnWebMediaPlayerCreated() {
  if (auto* vfc_requester = VideoFrameCallbackRequester::From(*this))
    vfc_requester->OnWebMediaPlayerCreated();
}

void HTMLVideoElement::OnWebMediaPlayerCleared() {
  if (auto* vfc_requester = VideoFrameCallbackRequester::From(*this))
    vfc_requester->OnWebMediaPlayerCleared();

  UpdateVideoVisibilityTracker();
}

void HTMLVideoElement::RecordVideoOcclusionState(
    std::string_view occlusion_state) const {
  if (!GetWebMediaPlayer()) {
    return;
  }

  GetWebMediaPlayer()->RecordVideoOcclusionState(occlusion_state);
}

void HTMLVideoElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == html_names::kDisablepictureinpictureAttr)
    UpdatePictureInPictureAvailability();
}

void HTMLVideoElement::OnRequestVideoFrameCallback() {
  if (auto* vfc_requester = VideoFrameCallbackRequester::From(*this)) {
    vfc_requester->OnRequestVideoFrameCallback();
  }
}

}  // namespace blink
