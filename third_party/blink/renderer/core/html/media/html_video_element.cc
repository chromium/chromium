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
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
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

// This enum is used to record histograms. Do not reorder.
enum VideoPersistenceControlsType {
  kNative = 0,
  kCustom = 1,
  kMaxValue = 1,
};

}  // anonymous namespace

HTMLVideoElement::HTMLVideoElement(Document& document)
    : HTMLMediaElement(html_names::kVideoTag, document),
      remoting_interstitial_(nullptr),
      picture_in_picture_interstitial_(nullptr),
      is_persistent_(false),
      is_auto_picture_in_picture_(false),
      is_effectively_fullscreen_(false),
      is_default_overridden_intrinsic_size_(
          !document.IsMediaDocument() && GetExecutionContext() &&
          !GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::DocumentPolicyFeature::kUnsizedMedia)),
      video_has_played_(false),
      mostly_filling_viewport_(false) {
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
  visitor->Trace(wake_lock_);
  visitor->Trace(remoting_interstitial_);
  visitor->Trace(picture_in_picture_interstitial_);
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

  return HTMLMediaElement::InsertedInto(insertion_point);
}

void HTMLVideoElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLMediaElement::RemovedFrom(insertion_point);
  custom_controls_fullscreen_detector_->Detach();

  SetPersistentState(false);
}

void HTMLVideoElement::ContextDestroyed() {
  custom_controls_fullscreen_detector_->ContextDestroyed();
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
  UpdatePosterImage();
}

void HTMLVideoElement::UpdatePosterImage() {
  ImageResourceContent* image_content = nullptr;

  // Load the poster if set, |VideoLayout| will decide whether to draw it.
  if (!PosterImageURL().IsEmpty()) {
    if (!image_loader_)
      image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
    image_loader_->UpdateFromElement();
    image_content = image_loader_->GetContent();
  }

  if (GetLayoutObject()) {
    To<LayoutImage>(GetLayoutObject())
        ->ImageResource()
        ->SetImageResource(image_content);
    UpdateLayoutObject();
  }
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
    UpdatePosterImage();

    // Notify the player when the poster image URL changes.
    if (GetWebMediaPlayer())
      GetWebMediaPlayer()->SetPoster(PosterImageURL());

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
  if (is_default_overridden_intrinsic_size_)
    return LayoutReplaced::kDefaultWidth;
  if (!GetWebMediaPlayer())
    return 0;
  return GetWebMediaPlayer()->NaturalSize().width();
}

unsigned HTMLVideoElement::videoHeight() const {
  if (is_default_overridden_intrinsic_size_)
    return LayoutReplaced::kDefaultHeight;
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
    // Record the type of video. If it is already fullscreen, it is a video with
    // native controls, otherwise it is assumed to be with custom controls.
    // This is only recorded when entering this mode.
    base::UmaHistogramEnumeration("Media.VideoPersistence.ControlsType",
                                  IsFullscreen()
                                      ? VideoPersistenceControlsType::kNative
                                      : VideoPersistenceControlsType::kCustom);

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

bool HTMLVideoElement::IsPersistent() const {
  return is_persistent_;
}

void HTMLVideoElement::OnPlay() {
  if (!video_has_played_) {
    video_has_played_ = true;
    UpdatePictureInPictureAvailability();
  }

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
        /* (root) margin */ Vector<Length>(),
        /* scroll_margin */ Vector<Length>(),
        /* thresholds */ {IntersectionObserver::kMinimumThreshold},
        /* document */ &GetDocument(),
        /* callback */
        WTF::BindRepeating(&HTMLVideoElement::OnIntersectionChangedForLazyLoad,
                           WrapWeakPersistent(this)),
        /* ukm_metric_id */
        LocalFrameUkmAggregator::kMediaIntersectionObserver);
    lazy_load_intersection_observer_->observe(this);
  }

  UpdatePictureInPictureAvailability();
}

void HTMLVideoElement::RequestEnterPictureInPicture() {
  PictureInPictureController::From(GetDocument())
      .EnterPictureInPicture(this, /*promise=*/nullptr);
}

void HTMLVideoElement::RequestMediaRemoting() {
  GetWebMediaPlayer()->RequestMediaRemoting();
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
  if (!base::FeatureList::IsEnabled(features::kLCPVideoFirstFrame)) {
    return;
  }
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
  HTMLMediaElement::DidMoveToNewDocument(old_document);
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
    bool allow_accelerated_images) {
  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  scoped_refptr<media::VideoFrame> media_video_frame;
  if (auto* wmp = GetWebMediaPlayer()) {
    media_video_frame = wmp->GetCurrentFrameThenUpdate();
    video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!media_video_frame || !video_renderer)
    return nullptr;

  // TODO(https://crbug.com/1341235): The choice of color type, alpha type,
  // and color space is inappropriate in many circumstances.
  const auto resource_provider_info =
      SkImageInfo::Make(gfx::SizeToSkISize(media_video_frame->natural_size()),
                        kN32_SkColorType, kPremul_SkAlphaType, nullptr);
  if (!resource_provider_ ||
      allow_accelerated_images != resource_provider_->IsAccelerated() ||
      resource_provider_info != resource_provider_->GetSkImageInfo()) {
    viz::RasterContextProvider* raster_context_provider = nullptr;
    if (allow_accelerated_images) {
      if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
        if (auto* context_provider = wrapper->ContextProvider())
          raster_context_provider = context_provider->RasterContextProvider();
      }
    }
    // Providing a null |raster_context_provider| creates a software provider.
    resource_provider_ = CreateResourceProviderForVideoFrame(
        resource_provider_info, raster_context_provider);
    if (!resource_provider_)
      return nullptr;
  }

  const auto dest_rect = gfx::Rect(media_video_frame->natural_size());
  auto image = CreateImageFromVideoFrame(std::move(media_video_frame),
                                         /*allow_zero_copy_images=*/true,
                                         resource_provider_.get(),
                                         video_renderer, dest_rect);
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

ScriptPromise HTMLVideoElement::CreateImageBitmap(
    ScriptState* script_state,
    absl::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (getNetworkState() == HTMLMediaElement::kNetworkEmpty) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The provided element has not retrieved data.");
    return ScriptPromise();
  }
  if (!HasAvailableVideoFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The provided element's player has no current data.");
    return ScriptPromise();
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

  if (RuntimeEnabledFeatures::CSSPictureInPictureEnabled())
    PseudoStateChanged(CSSSelector::kPseudoPictureInPicture);

  DCHECK(GetWebMediaPlayer());
  GetWebMediaPlayer()->OnDisplayTypeChanged(GetDisplayType());
}

void HTMLVideoElement::OnExitedPictureInPicture() {
  if (picture_in_picture_interstitial_)
    picture_in_picture_interstitial_->Hide();

  if (RuntimeEnabledFeatures::CSSPictureInPictureEnabled())
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
