// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_info.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NullImageResourceInfo final
    : public GarbageCollected<NullImageResourceInfo>,
      public ImageResourceInfo {
 public:
  NullImageResourceInfo() = default;

  void Trace(Visitor* visitor) const override {
    ImageResourceInfo::Trace(visitor);
  }

 private:
  const KURL& Url() const override { return url_; }
  base::TimeTicks LoadResponseEnd() const override { return base::TimeTicks(); }
  base::TimeTicks LoadStart() const override { return base::TimeTicks(); }
  base::TimeTicks LoadEnd() const override { return base::TimeTicks(); }
  base::TimeTicks DiscoveryTime() const override { return base::TimeTicks(); }
  const ResourceResponse& GetResponse() const override { return response_; }
  bool IsCacheValidator() const override { return false; }
  bool IsAccessAllowed(
      DoesCurrentFrameHaveSingleSecurityOrigin) const override {
    return true;
  }
  bool HasCacheControlNoStoreHeader() const override { return false; }
  std::optional<ResourceError> GetResourceError() const override {
    return std::nullopt;
  }

  void SetDecodedSize(size_t) override {}
  void WillAddClientOrObserver() override {}
  void DidRemoveClientOrObserver() override {}
  void EmulateLoadStartedForInspector(
      ResourceFetcher*,
      const AtomicString& initiator_name) override {}

  void LoadDeferredImage(ResourceFetcher* fetcher) override {}

  bool IsAdResource() const override { return false; }

  const HashSet<String>* GetUnsupportedImageMimeTypes() const override {
    return nullptr;
  }

  std::optional<WebURLRequest::Priority> RequestPriority() const override {
    return std::nullopt;
  }

  const KURL url_;
  const ResourceResponse response_;
};

}  // namespace

ImageResourceContent::ImageResourceContent(scoped_refptr<blink::Image> image)
    : image_(std::move(image)) {
  DEFINE_STATIC_LOCAL(Persistent<NullImageResourceInfo>, null_info,
                      (MakeGarbageCollected<NullImageResourceInfo>()));
  info_ = null_info;
}

ImageResourceContent* ImageResourceContent::CreateLoaded(
    scoped_refptr<blink::Image> image) {
  DCHECK(image);
  ImageResourceContent* content =
      MakeGarbageCollected<ImageResourceContent>(std::move(image));
  content->content_status_ = ResourceStatus::kCached;
  return content;
}

ImageResourceContent* ImageResourceContent::Fetch(FetchParameters& params,
                                                  ResourceFetcher* fetcher) {
  // TODO(hiroshige): Remove direct references to ImageResource by making
  // the dependencies around ImageResource and ImageResourceContent cleaner.
  ImageResource* resource = ImageResource::Fetch(params, fetcher);
  if (!resource)
    return nullptr;

  return resource->GetContent();
}

void ImageResourceContent::SetImageResourceInfo(ImageResourceInfo* info) {
  info_ = info;
}

void ImageResourceContent::Trace(Visitor* visitor) const {
  visitor->Trace(info_);
  visitor->Trace(observers_);
  visitor->Trace(finished_observers_);
  ImageObserver::Trace(visitor);
  MediaTiming::Trace(visitor);
}

void ImageResourceContent::HandleObserverFinished(
    ImageResourceObserver* observer) {
  {
    ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
        this);
    auto it = observers_.find(observer);
    if (it != observers_.end()) {
      observers_.erase(it);
      finished_observers_.insert(observer);
    }
  }
  observer->ImageNotifyFinished(this);
  UpdateImageAnimationPolicy();
}

void ImageResourceContent::AddObserver(ImageResourceObserver* observer) {
  CHECK(!is_add_remove_observer_prohibited_);

  info_->WillAddClientOrObserver();

  {
    ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
        this);
    observers_.insert(observer);
  }

  if (info_->IsCacheValidator())
    return;

  if (image_ && !image_->IsNull()) {
    observer->ImageChanged(this, CanDeferInvalidation::kNo);
  }

  if (IsSufficientContentLoadedForPaint() && observers_.Contains(observer))
    HandleObserverFinished(observer);
}

void ImageResourceContent::RemoveObserver(ImageResourceObserver* observer) {
  DCHECK(observer);
  CHECK(!is_add_remove_observer_prohibited_);
  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);

  auto it = observers_.find(observer);
  bool fully_erased;
  if (it != observers_.end()) {
    fully_erased = observers_.erase(it) && finished_observers_.find(observer) ==
                                               finished_observers_.end();
  } else {
    it = finished_observers_.find(observer);
    CHECK(it != finished_observers_.end(), base::NotFatalUntil::M130);
    fully_erased = finished_observers_.erase(it);
  }
  DidRemoveObserver();
  if (fully_erased)
    observer->NotifyImageFullyRemoved(this);
}

void ImageResourceContent::DidRemoveObserver() {
  info_->DidRemoveClientOrObserver();
}

static void PriorityFromObserver(
    const ImageResourceObserver* observer,
    ResourcePriority& priority,
    ResourcePriority& priority_excluding_image_loader) {
  ResourcePriority next_priority = observer->ComputeResourcePriority();
  if (next_priority.is_lcp_resource) {
    // Mark the resource as predicted LCP despite its visibility.
    priority.is_lcp_resource = true;
    priority_excluding_image_loader.is_lcp_resource = true;
  }

  if (next_priority.visibility == ResourcePriority::kNotVisible)
    return;

  priority.visibility = ResourcePriority::kVisible;
  priority.intra_priority_value += next_priority.intra_priority_value;

  if (next_priority.source != ResourcePriority::Source::kImageLoader) {
    priority_excluding_image_loader.visibility = ResourcePriority::kVisible;
    priority_excluding_image_loader.intra_priority_value +=
        next_priority.intra_priority_value;
  }
}

std::pair<ResourcePriority, ResourcePriority>
ImageResourceContent::PriorityFromObservers() const {
  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);
  ResourcePriority priority;
  ResourcePriority priority_excluding_image_loader;

  for (const auto& it : finished_observers_)
    PriorityFromObserver(it.key, priority, priority_excluding_image_loader);
  for (const auto& it : observers_)
    PriorityFromObserver(it.key, priority, priority_excluding_image_loader);

  return std::make_pair(priority, priority_excluding_image_loader);
}

std::optional<WebURLRequest::Priority> ImageResourceContent::RequestPriority()
    const {
  return info_->RequestPriority();
}

void ImageResourceContent::DestroyDecodedData() {
  if (!image_)
    return;
  CHECK(!ErrorOccurred());
  image_->DestroyDecodedData();
}

void ImageResourceContent::DoResetAnimation() {
  if (image_)
    image_->ResetAnimation();
}

scoped_refptr<const SharedBuffer> ImageResourceContent::ResourceBuffer() const {
  if (image_)
    return image_->Data();
  return nullptr;
}

bool ImageResourceContent::ShouldUpdateImageImmediately() const {
  // If we don't have the size available yet, then update immediately since
  // we need to know the image size as soon as possible. Likewise for
  // animated images, update right away since we shouldn't throttle animated
  // images.
  return size_available_ == Image::kSizeUnavailable ||
         (image_ && image_->MaybeAnimated());
}

blink::Image* ImageResourceContent::GetImage() const {
  if (!image_ || ErrorOccurred())
    return Image::NullImage();

  return image_.get();
}

gfx::Size ImageResourceContent::IntrinsicSize(
    RespectImageOrientationEnum should_respect_image_orientation) const {
  if (!image_)
    return gfx::Size();
  RespectImageOrientationEnum respect_orientation =
      ForceOrientationIfNecessary(should_respect_image_orientation);
  return image_->Size(respect_orientation);
}

RespectImageOrientationEnum ImageResourceContent::ForceOrientationIfNecessary(
    RespectImageOrientationEnum default_orientation) const {
  if (image_ && image_->IsBitmapImage() && !IsAccessAllowed())
    return kRespectImageOrientation;
  return default_orientation;
}

void ImageResourceContent::NotifyObservers(
    NotifyFinishOption notifying_finish_option,
    CanDeferInvalidation defer) {
  {
    HeapVector<Member<ImageResourceObserver>> finished_observers_as_vector;
    {
      ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
          this);
      CopyToVector(finished_observers_, finished_observers_as_vector);
    }

    for (ImageResourceObserver* observer : finished_observers_as_vector) {
      if (finished_observers_.Contains(observer))
        observer->ImageChanged(this, defer);
    }
  }
  {
    HeapVector<Member<ImageResourceObserver>> observers_as_vector;
    {
      ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
          this);
      CopyToVector(observers_, observers_as_vector);
    }

    for (ImageResourceObserver* observer : observers_as_vector) {
      if (observers_.Contains(observer)) {
        observer->ImageChanged(this, defer);
        if (notifying_finish_option == kShouldNotifyFinish &&
            observers_.Contains(observer)) {
          HandleObserverFinished(observer);
        }
      }
    }
  }
}

scoped_refptr<Image> ImageResourceContent::CreateImage(bool is_multipart) {
  String content_dpr_value =
      info_->GetResponse().HttpHeaderField(http_names::kContentDPR);
  wtf_size_t comma = content_dpr_value.ReverseFind(',');
  if (comma != kNotFound && comma < content_dpr_value.length() - 1) {
    content_dpr_value = content_dpr_value.Substring(comma + 1);
  }
  device_pixel_ratio_header_value_ =
      content_dpr_value.ToFloat(&has_device_pixel_ratio_header_value_);
  if (!has_device_pixel_ratio_header_value_ ||
      device_pixel_ratio_header_value_ <= 0.0) {
    device_pixel_ratio_header_value_ = 1.0;
    has_device_pixel_ratio_header_value_ = false;
  }
  if (info_->GetResponse().MimeType() == "image/svg+xml")
    return SVGImage::Create(this, is_multipart);
  return BitmapImage::Create(this, is_multipart);
}

void ImageResourceContent::ClearImage() {
  if (!image_)
    return;

  // If our Image has an observer, it's always us so we need to clear the back
  // pointer before dropping our reference.
  image_->ClearImageObserver();
  image_ = nullptr;
  size_available_ = Image::kSizeUnavailable;
}

// |new_status| is the status of corresponding ImageResource.
void ImageResourceContent::UpdateToLoadedContentStatus(
    ResourceStatus new_status) {
  // When |ShouldNotifyFinish|, we set content_status_
  // to a loaded ResourceStatus.

  // Checks |new_status| (i.e. Resource's current status).
  switch (new_status) {
    case ResourceStatus::kCached:
    case ResourceStatus::kPending:
      // In case of successful load, Resource's status can be
      // kCached (e.g. for second part of multipart image) or
      // still Pending (e.g. for a non-multipart image).
      // Therefore we use kCached as the new state here.
      new_status = ResourceStatus::kCached;
      break;

    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // In case of error, Resource's status is set to an error status
      // before UpdateImage() and thus we use the error status as-is.
      break;

    case ResourceStatus::kNotStarted:
      CHECK(false);
      break;
  }

  // Updates the status.
  content_status_ = new_status;
}

void ImageResourceContent::NotifyStartLoad() {
  // Checks ImageResourceContent's previous status.
  switch (GetContentStatus()) {
    case ResourceStatus::kPending:
      CHECK(false);
      break;

    case ResourceStatus::kNotStarted:
      // Normal load start.
      break;

    case ResourceStatus::kCached:
    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // Load start due to revalidation/reload.
      break;
  }

  content_status_ = ResourceStatus::kPending;
}

void ImageResourceContent::AsyncLoadCompleted(const blink::Image* image) {
  if (image_ != image)
    return;
  CHECK_EQ(size_available_, Image::kSizeAvailableAndLoadingAsynchronously);
  size_available_ = Image::kSizeAvailable;
  UpdateToLoadedContentStatus(ResourceStatus::kCached);
  NotifyObservers(kShouldNotifyFinish, CanDeferInvalidation::kNo);
}

ImageResourceContent::UpdateImageResult ImageResourceContent::UpdateImage(
    scoped_refptr<SharedBuffer> data,
    ResourceStatus status,
    UpdateImageOption update_image_option,
    bool all_data_received,
    bool is_multipart) {
  TRACE_EVENT0("blink", "ImageResourceContent::updateImage");

#if DCHECK_IS_ON()
  DCHECK(!is_update_image_being_called_);
  base::AutoReset<bool> scope(&is_update_image_being_called_, true);
#endif

  // Clears the existing image, if instructed by |updateImageOption|.
  switch (update_image_option) {
    case kClearAndUpdateImage:
    case kClearImageAndNotifyObservers:
      ClearImage();
      break;
    case kUpdateImage:
      break;
  }

  // Updates the image, if instructed by |updateImageOption|.
  switch (update_image_option) {
    case kClearImageAndNotifyObservers:
      DCHECK(!data);
      break;

    case kUpdateImage:
    case kClearAndUpdateImage:
      // Have the image update its data from its internal buffer. It will not do
      // anything now, but will delay decoding until queried for info (like size
      // or specific image frames).
      if (data) {
        if (!image_)
          image_ = CreateImage(is_multipart);
        DCHECK(image_);
        size_available_ = image_->SetData(std::move(data), all_data_received);
        DCHECK(all_data_received ||
               size_available_ !=
                   Image::kSizeAvailableAndLoadingAsynchronously);
      }

      // Go ahead and tell our observers to try to draw if we have either
      // received all the data or the size is known. Each chunk from the network
      // causes observers to repaint, which will force that chunk to decode.
      if (size_available_ == Image::kSizeUnavailable && !all_data_received)
        return UpdateImageResult::kNoDecodeError;

      if (image_) {
        // Mime type could be null, see https://crbug.com/1485926.
        if (!image_->MimeType()) {
          return UpdateImageResult::kShouldDecodeError;
        }
        const HashSet<String>* unsupported_mime_types =
            info_->GetUnsupportedImageMimeTypes();
        if (unsupported_mime_types &&
            unsupported_mime_types->Contains(image_->MimeType())) {
          return UpdateImageResult::kShouldDecodeError;
        }
      }

      // As per spec, zero intrinsic size SVG is a valid image so do not
      // consider such an image as DecodeError.
      // https://www.w3.org/TR/SVG/struct.html#SVGElementWidthAttribute
      if (!image_ ||
          (image_->IsNull() && (!IsA<SVGImage>(image_.get()) ||
                                size_available_ == Image::kSizeUnavailable))) {
        ClearImage();
        return UpdateImageResult::kShouldDecodeError;
      }
      break;
  }

  DCHECK(all_data_received ||
         size_available_ != Image::kSizeAvailableAndLoadingAsynchronously);

  // Notifies the observers.
  // It would be nice to only redraw the decoded band of the image, but with the
  // current design (decoding delayed until painting) that seems hard.
  //
  // In the case of kSizeAvailableAndLoadingAsynchronously, we are waiting for
  // SVG image completion, and thus we notify observers of kDoNotNotifyFinish
  // here, and will notify observers of finish later in AsyncLoadCompleted().
  //
  // Don't allow defering of invalidation if it resulted from a data update.
  // This is necessary to ensure that all PaintImages in a recording committed
  // to the compositor have the same data.
  if (all_data_received &&
      size_available_ != Image::kSizeAvailableAndLoadingAsynchronously) {
    UpdateToLoadedContentStatus(status);
    NotifyObservers(kShouldNotifyFinish, CanDeferInvalidation::kNo);
  } else {
    NotifyObservers(kDoNotNotifyFinish, CanDeferInvalidation::kNo);
  }

  return UpdateImageResult::kNoDecodeError;
}

ImageDecoder::CompressionFormat ImageResourceContent::GetCompressionFormat()
    const {
  if (!image_)
    return ImageDecoder::kUndefinedFormat;
  return ImageDecoder::GetCompressionFormat(image_->Data(),
                                            GetResponse().HttpContentType());
}

uint64_t ImageResourceContent::ContentSizeForEntropy() const {
  int64_t resource_length = GetResponse().ExpectedContentLength();
  if (resource_length <= 0) {
    if (image_ && image_->HasData()) {
      // WPT and LayoutTests server returns -1 or 0 for the content length.
      resource_length = image_->DataSize();
    } else {
      resource_length = 0;
    }
  }
  return resource_length;
}

void ImageResourceContent::DecodedSizeChangedTo(const blink::Image* image,
                                                size_t new_size) {
  if (!image || image != image_)
    return;

  info_->SetDecodedSize(new_size);
}

bool ImageResourceContent::ShouldPauseAnimation(const blink::Image* image) {
  if (!image || image != image_)
    return false;

  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);

  for (const auto& it : finished_observers_) {
    if (it.key->WillRenderImage())
      return false;
  }

  for (const auto& it : observers_) {
    if (it.key->WillRenderImage())
      return false;
  }

  return true;
}

void ImageResourceContent::UpdateImageAnimationPolicy() {
  if (!image_)
    return;

  mojom::blink::ImageAnimationPolicy new_policy =
      mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
  {
    ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
        this);
    for (const auto& it : finished_observers_) {
      if (it.key->GetImageAnimationPolicy(new_policy))
        break;
    }
    for (const auto& it : observers_) {
      if (it.key->GetImageAnimationPolicy(new_policy))
        break;
    }
  }

  image_->SetAnimationPolicy(new_policy);
}

void ImageResourceContent::Changed(const blink::Image* image) {
  if (!image || image != image_)
    return;
  NotifyObservers(kDoNotNotifyFinish, CanDeferInvalidation::kYes);
}

bool ImageResourceContent::IsAccessAllowed() const {
  return info_->IsAccessAllowed(
      GetImage()->CurrentFrameHasSingleSecurityOrigin()
          ? ImageResourceInfo::kHasSingleSecurityOrigin
          : ImageResourceInfo::kHasMultipleSecurityOrigin);
}

void ImageResourceContent::EmulateLoadStartedForInspector(
    ResourceFetcher* fetcher,
    const AtomicString& initiator_name) {
  info_->EmulateLoadStartedForInspector(fetcher, initiator_name);
}

void ImageResourceContent::SetIsSufficientContentLoadedForPaint() {
  NOTREACHED_IN_MIGRATION();
}

bool ImageResourceContent::IsSufficientContentLoadedForPaint() const {
  return IsLoaded();
}

bool ImageResourceContent::IsLoaded() const {
  return GetContentStatus() > ResourceStatus::kPending;
}

bool ImageResourceContent::IsLoading() const {
  return GetContentStatus() == ResourceStatus::kPending;
}

bool ImageResourceContent::ErrorOccurred() const {
  return GetContentStatus() == ResourceStatus::kLoadError ||
         GetContentStatus() == ResourceStatus::kDecodeError;
}

bool ImageResourceContent::LoadFailedOrCanceled() const {
  return GetContentStatus() == ResourceStatus::kLoadError;
}

ResourceStatus ImageResourceContent::GetContentStatus() const {
  return content_status_;
}

bool ImageResourceContent::IsAnimatedImage() const {
  return image_ && !image_->IsNull() && image_->MaybeAnimated();
}

bool ImageResourceContent::IsPaintedFirstFrame() const {
  return IsAnimatedImage() && image_->CurrentFrameIsComplete();
}

bool ImageResourceContent::TimingAllowPassed() const {
  return GetResponse().TimingAllowPassed();
}

// TODO(hiroshige): Consider removing the following methods, or stopping
// redirecting to ImageResource.
const KURL& ImageResourceContent::Url() const {
  return info_->Url();
}

bool ImageResourceContent::IsDataUrl() const {
  return Url().ProtocolIsData();
}

AtomicString ImageResourceContent::MediaType() const {
  if (!image_)
    return AtomicString();
  return AtomicString(image_->FilenameExtension());
}

void ImageResourceContent::SetIsBroken() {
  is_broken_ = true;
}

bool ImageResourceContent::IsBroken() const {
  return is_broken_;
}

base::TimeTicks ImageResourceContent::DiscoveryTime() const {
  return info_->DiscoveryTime();
}

base::TimeTicks ImageResourceContent::LoadStart() const {
  return info_->LoadStart();
}

base::TimeTicks ImageResourceContent::LoadEnd() const {
  return info_->LoadEnd();
}

base::TimeTicks ImageResourceContent::LoadResponseEnd() const {
  return info_->LoadResponseEnd();
}

bool ImageResourceContent::HasCacheControlNoStoreHeader() const {
  return info_->HasCacheControlNoStoreHeader();
}

float ImageResourceContent::DevicePixelRatioHeaderValue() const {
  return device_pixel_ratio_header_value_;
}

bool ImageResourceContent::HasDevicePixelRatioHeaderValue() const {
  return has_device_pixel_ratio_header_value_;
}

const ResourceResponse& ImageResourceContent::GetResponse() const {
  return info_->GetResponse();
}

std::optional<ResourceError> ImageResourceContent::GetResourceError() const {
  return info_->GetResourceError();
}

bool ImageResourceContent::IsCacheValidator() const {
  return info_->IsCacheValidator();
}

void ImageResourceContent::LoadDeferredImage(ResourceFetcher* fetcher) {
  info_->LoadDeferredImage(fetcher);
}

bool ImageResourceContent::IsAdResource() const {
  return info_->IsAdResource();
}

void ImageResourceContent::RecordDecodedImageType(UseCounter* use_counter) {
  if (auto* bitmap_image = DynamicTo<BitmapImage>(image_.get()))
    bitmap_image->RecordDecodedImageType(use_counter);
}

}  // namespace blink
