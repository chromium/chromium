/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/loader/resource/image_resource.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_info.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#include "v8/include/v8.h"

namespace blink {

namespace {

// The amount of time to wait before informing the clients that the image has
// been updated (in seconds). This effectively throttles invalidations that
// result from new data arriving for this image.
constexpr auto kFlushDelay = base::TimeDelta::FromSeconds(1);

}  // namespace

class ImageResource::ImageResourceInfoImpl final
    : public GarbageCollected<ImageResourceInfoImpl>,
      public ImageResourceInfo {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResourceInfoImpl);

 public:
  explicit ImageResourceInfoImpl(ImageResource* resource)
      : resource_(resource) {
    DCHECK(resource_);
  }
  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(resource_);
    ImageResourceInfo::Trace(visitor);
  }

 private:
  const KURL& Url() const override { return resource_->Url(); }
  base::TimeTicks LoadResponseEnd() const override {
    return resource_->LoadResponseEnd();
  }
  bool IsSchedulingReload() const override {
    return resource_->is_scheduling_reload_;
  }
  const ResourceResponse& GetResponse() const override {
    return resource_->GetResponse();
  }
  bool ShouldShowPlaceholder() const override {
    return resource_->ShouldShowPlaceholder();
  }
  bool ShouldShowLazyImagePlaceholder() const override {
    return resource_->ShouldShowLazyImagePlaceholder();
  }
  bool IsCacheValidator() const override {
    return resource_->IsCacheValidator();
  }
  bool SchedulingReloadOrShouldReloadBrokenPlaceholder() const override {
    return resource_->is_scheduling_reload_ ||
           resource_->ShouldReloadBrokenPlaceholder();
  }
  bool IsAccessAllowed(
      DoesCurrentFrameHaveSingleSecurityOrigin
          does_current_frame_has_single_security_origin) const override {
    return resource_->IsAccessAllowed(
        does_current_frame_has_single_security_origin);
  }
  bool HasCacheControlNoStoreHeader() const override {
    return resource_->HasCacheControlNoStoreHeader();
  }
  base::Optional<ResourceError> GetResourceError() const override {
    if (resource_->LoadFailedOrCanceled())
      return resource_->GetResourceError();
    return base::nullopt;
  }

  void SetDecodedSize(size_t size) override { resource_->SetDecodedSize(size); }
  void WillAddClientOrObserver() override {
    resource_->WillAddClientOrObserver();
  }
  void DidRemoveClientOrObserver() override {
    resource_->DidRemoveClientOrObserver();
  }
  void EmulateLoadStartedForInspector(
      ResourceFetcher* fetcher,
      const KURL& url,
      const AtomicString& initiator_name) override {
    fetcher->EmulateLoadStartedForInspector(
        resource_.Get(), url, mojom::RequestContextType::IMAGE, initiator_name);
  }

  void LoadDeferredImage(ResourceFetcher* fetcher) override {
    if (resource_->GetType() == ResourceType::kImage &&
        resource_->StillNeedsLoad() &&
        !fetcher->ShouldDeferImageLoad(resource_->Url())) {
      fetcher->StartLoad(resource_);
    }
  }

  const Member<ImageResource> resource_;
};

class ImageResource::ImageResourceFactory : public NonTextResourceFactory {
  STACK_ALLOCATED();

 public:
  explicit ImageResourceFactory(const FetchParameters& fetch_params)
      : NonTextResourceFactory(ResourceType::kImage),
        fetch_params_(&fetch_params) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options) const override {
    return MakeGarbageCollected<ImageResource>(
        request, options, ImageResourceContent::CreateNotStarted(),
        fetch_params_->GetImageRequestOptimization() ==
            FetchParameters::kAllowPlaceholder);
  }

 private:
  // Weak, unowned pointer. Must outlive |this|.
  const FetchParameters* fetch_params_;
};

ImageResource* ImageResource::Fetch(FetchParameters& params,
                                    ResourceFetcher* fetcher) {
  if (params.GetResourceRequest().GetRequestContext() ==
      mojom::RequestContextType::UNSPECIFIED) {
    params.SetRequestContext(mojom::RequestContextType::IMAGE);
  }

  ImageResource* resource = ToImageResource(
      fetcher->RequestResource(params, ImageResourceFactory(params), nullptr));

  // If the fetch originated from user agent CSS we should mark it as a user
  // agent resource.
  if (params.Options().initiator_info.name ==
      fetch_initiator_type_names::kUacss)
    resource->FlagAsUserAgentResource();
  return resource;
}

Resource::MatchStatus ImageResource::CanReuse(
    const FetchParameters& params) const {
  // If the image is a placeholder, but this fetch doesn't allow a
  // placeholder, then do not reuse this resource.
  if (params.GetImageRequestOptimization() !=
          FetchParameters::kAllowPlaceholder &&
      placeholder_option_ != PlaceholderOption::kDoNotReloadPlaceholder) {
    return MatchStatus::kImagePlaceholder;
  }

  return Resource::CanReuse(params);
}

bool ImageResource::CanUseCacheValidator() const {
  // Disable revalidation while ImageResourceContent is still waiting for
  // SVG load completion.
  // TODO(hiroshige): Clean up revalidation-related dependencies.
  if (!GetContent()->IsLoaded())
    return false;

  return Resource::CanUseCacheValidator();
}

ImageResource* ImageResource::Create(const ResourceRequest& request) {
  ResourceLoaderOptions options;
  return MakeGarbageCollected<ImageResource>(
      request, options, ImageResourceContent::CreateNotStarted(), false);
}

ImageResource* ImageResource::CreateForTest(const KURL& url) {
  ResourceRequest request(url);
  request.SetInspectorId(CreateUniqueIdentifier());
  return Create(request);
}

ImageResource::ImageResource(const ResourceRequest& resource_request,
                             const ResourceLoaderOptions& options,
                             ImageResourceContent* content,
                             bool is_placeholder)
    : Resource(resource_request, ResourceType::kImage, options),
      content_(content),
      is_scheduling_reload_(false),
      placeholder_option_(
          is_placeholder ? PlaceholderOption::kShowAndReloadPlaceholderAlways
                         : PlaceholderOption::kDoNotReloadPlaceholder) {
  DCHECK(GetContent());
  RESOURCE_LOADING_DVLOG(1)
      << "MakeGarbageCollected<ImageResource>(ResourceRequest) " << this;
  GetContent()->SetImageResourceInfo(
      MakeGarbageCollected<ImageResourceInfoImpl>(this));
}

ImageResource::~ImageResource() {
  RESOURCE_LOADING_DVLOG(1) << "~ImageResource " << this;

  if (is_referenced_from_ua_stylesheet_)
    InstanceCounters::DecrementCounter(InstanceCounters::kUACSSResourceCounter);
}

void ImageResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                 WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/image_content";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  if (content_->HasImage() && content_->GetImage()->Data())
    dump->AddScalar("size", "bytes", content_->GetImage()->Data()->size());
}

void ImageResource::Trace(blink::Visitor* visitor) {
  visitor->Trace(multipart_parser_);
  visitor->Trace(content_);
  Resource::Trace(visitor);
  MultipartImageResourceParser::Client::Trace(visitor);
}

void ImageResource::NotifyFinished() {
  // Don't notify clients of completion if this ImageResource is
  // about to be reloaded.
  if (is_scheduling_reload_ || ShouldReloadBrokenPlaceholder())
    return;

  Resource::NotifyFinished();
}

bool ImageResource::HasClientsOrObservers() const {
  return Resource::HasClientsOrObservers() || GetContent()->HasObservers();
}

void ImageResource::DidAddClient(ResourceClient* client) {
  DCHECK((multipart_parser_ && IsLoading()) || !Data() ||
         GetContent()->HasImage());

  // Don't notify observers and clients of completion if this ImageResource is
  // about to be reloaded.
  if (is_scheduling_reload_ || ShouldReloadBrokenPlaceholder())
    return;

  Resource::DidAddClient(client);
}

void ImageResource::DestroyDecodedDataForFailedRevalidation() {
  // Clears the image, as we must create a new image for the failed
  // revalidation response.
  UpdateImage(nullptr, ImageResourceContent::kClearAndUpdateImage, false);
  SetDecodedSize(0);
}

void ImageResource::DestroyDecodedDataIfPossible() {
  GetContent()->DestroyDecodedData();
  if (GetContent()->HasImage() && !IsUnusedPreload() &&
      GetContent()->IsRefetchableDataFromDiskCache()) {
    UMA_HISTOGRAM_MEMORY_KB(
        "Memory.Renderer.EstimatedDroppableEncodedSize",
        base::saturated_cast<base::Histogram::Sample>(EncodedSize() / 1024));
  }
}

void ImageResource::AllClientsAndObserversRemoved() {
  // After ErrorOccurred() is set true in Resource::FinishAsError() before
  // the subsequent UpdateImage() in ImageResource::FinishAsError(),
  // HasImage() is true and ErrorOccurred() is true.
  // |is_during_finish_as_error_| is introduced to allow such cases.
  // crbug.com/701723
  // TODO(hiroshige): Make the CHECK condition cleaner.
  CHECK(is_during_finish_as_error_ || !GetContent()->HasImage() ||
        !ErrorOccurred());
  GetContent()->DoResetAnimation();
  if (multipart_parser_)
    multipart_parser_->Cancel();
  Resource::AllClientsAndObserversRemoved();
}

scoped_refptr<const SharedBuffer> ImageResource::ResourceBuffer() const {
  if (Data())
    return Data();
  return GetContent()->ResourceBuffer();
}

void ImageResource::AppendData(const char* data, size_t length) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(length);
  if (multipart_parser_) {
    multipart_parser_->AppendData(data, SafeCast<wtf_size_t>(length));
  } else {
    Resource::AppendData(data, length);

    // Update the image immediately if needed.
    //
    // ImageLoader is not available when this image is loaded via ImageDocument.
    // In this case, as the task runner is not available, update the image
    // immediately.
    //
    // TODO(hajimehoshi): updating/flushing image should be throttled when
    // necessary, so such tasks should be done on a throttleable task runner.
    if (GetContent()->ShouldUpdateImageImmediately() || !Loader()) {
      UpdateImage(Data(), ImageResourceContent::kUpdateImage, false);
      return;
    }

    // For other cases, only update at |kFlushDelay| intervals. This
    // throttles how frequently we update |m_image| and how frequently we
    // inform the clients which causes an invalidation of this image. In other
    // words, we only invalidate this image every |kFlushDelay| seconds
    // while loading.
    if (!is_pending_flushing_) {
      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          Loader()->GetLoadingTaskRunner();
      base::TimeTicks now = base::TimeTicks::Now();
      if (last_flush_time_.is_null())
        last_flush_time_ = now;

      DCHECK_LE(last_flush_time_, now);
      base::TimeDelta flush_delay =
          std::max(base::TimeDelta(), last_flush_time_ - now + kFlushDelay);
      task_runner->PostDelayedTask(FROM_HERE,
                                   WTF::Bind(&ImageResource::FlushImageIfNeeded,
                                             WrapWeakPersistent(this)),
                                   flush_delay);
      is_pending_flushing_ = true;
    }
  }
}

void ImageResource::FlushImageIfNeeded() {
  // We might have already loaded the image fully, in which case we don't need
  // to call |updateImage()|.
  if (IsLoading()) {
    last_flush_time_ = base::TimeTicks::Now();
    UpdateImage(Data(), ImageResourceContent::kUpdateImage, false);
  }
  is_pending_flushing_ = false;
}

void ImageResource::DecodeError(bool all_data_received) {
  size_t size = EncodedSize();

  ClearData();
  SetEncodedSize(0);
  if (!ErrorOccurred())
    SetStatus(ResourceStatus::kDecodeError);

  if (multipart_parser_)
    multipart_parser_->Cancel();

  bool is_multipart = !!multipart_parser_;
  // Finishes loading if needed, and notifies observers.
  if (!all_data_received && Loader()) {
    // Observers are notified via ImageResource::finish().
    // TODO(hiroshige): Do not call didFinishLoading() directly.
    Loader()->AbortResponseBodyLoading();
    Loader()->DidFinishLoading(base::TimeTicks::Now(), size, size, size, false);
  } else {
    auto result = GetContent()->UpdateImage(
        nullptr, GetStatus(),
        ImageResourceContent::kClearImageAndNotifyObservers, all_data_received,
        is_multipart);
    DCHECK_EQ(result, ImageResourceContent::UpdateImageResult::kNoDecodeError);
  }

  GetMemoryCache()->Remove(this);
}

void ImageResource::UpdateImageAndClearBuffer() {
  UpdateImage(Data(), ImageResourceContent::kClearAndUpdateImage, true);
  ClearData();
}

void ImageResource::NotifyStartLoad() {
  Resource::NotifyStartLoad();
  CHECK_EQ(GetStatus(), ResourceStatus::kPending);
  GetContent()->NotifyStartLoad();
}

void ImageResource::Finish(base::TimeTicks load_finish_time,
                           base::SingleThreadTaskRunner* task_runner) {
  if (multipart_parser_) {
    if (!ErrorOccurred())
      multipart_parser_->Finish();
    if (Data())
      UpdateImageAndClearBuffer();
  } else {
    UpdateImage(Data(), ImageResourceContent::kUpdateImage, true);
    // As encoded image data can be created from m_image  (see
    // ImageResource::resourceBuffer(), we don't have to keep m_data. Let's
    // clear this. As for the lifetimes of m_image and m_data, see this
    // document:
    // https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
    ClearData();
  }
  Resource::Finish(load_finish_time, task_runner);
}

void ImageResource::FinishAsError(const ResourceError& error,
                                  base::SingleThreadTaskRunner* task_runner) {
  if (multipart_parser_)
    multipart_parser_->Cancel();
  // TODO(hiroshige): Move setEncodedSize() call to Resource::error() if it
  // is really needed, or remove it otherwise.
  SetEncodedSize(0);
  is_during_finish_as_error_ = true;
  Resource::FinishAsError(error, task_runner);
  is_during_finish_as_error_ = false;
  UpdateImage(nullptr, ImageResourceContent::kClearImageAndNotifyObservers,
              true);
}

// Determines if |response| likely contains the entire resource for the purposes
// of determining whether or not to show a placeholder, e.g. if the server
// responded with a full 200 response or if the full image is smaller than the
// requested range.
static bool IsEntireResource(const ResourceResponse& response) {
  if (response.HttpStatusCode() != 206)
    return true;

  int64_t first_byte_position = -1, last_byte_position = -1,
          instance_length = -1;
  return ParseContentRangeHeaderFor206(
             response.HttpHeaderField("Content-Range"), &first_byte_position,
             &last_byte_position, &instance_length) &&
         first_byte_position == 0 && last_byte_position + 1 == instance_length;
}

void ImageResource::ResponseReceived(const ResourceResponse& response) {
  DCHECK(!multipart_parser_);
  if (response.MimeType() == "multipart/x-mixed-replace") {
    Vector<char> boundary = network_utils::ParseMultipartBoundary(
        response.HttpHeaderField(http_names::kContentType));
    // If there's no boundary, just handle the request normally.
    if (!boundary.IsEmpty()) {
      multipart_parser_ = MakeGarbageCollected<MultipartImageResourceParser>(
          response, boundary, this);
    }
  }

  // Notify the base class that a response has been received. Note that after
  // this call, |GetResponse()| will represent the full effective
  // ResourceResponse, while |response| might just be a revalidation response
  // (e.g. a 304) with a partial set of updated headers that were folded into
  // the cached response.
  Resource::ResponseReceived(response);

  if (placeholder_option_ ==
          PlaceholderOption::kShowAndReloadPlaceholderAlways &&
      IsEntireResource(GetResponse())) {
    if (GetResponse().HttpStatusCode() < 400 ||
        GetResponse().HttpStatusCode() >= 600) {
      // Don't treat a complete and broken image as a placeholder if the
      // response code is something other than a 4xx or 5xx error.
      // This is done to prevent reissuing the request in cases like
      // "204 No Content" responses to tracking requests triggered by <img>
      // tags, and <img> tags used to preload non-image resources.
      placeholder_option_ = PlaceholderOption::kDoNotReloadPlaceholder;
    } else {
      placeholder_option_ = PlaceholderOption::kReloadPlaceholderOnDecodeError;
    }
  }
}

bool ImageResource::ShouldShowPlaceholder() const {
  switch (placeholder_option_) {
    case PlaceholderOption::kShowAndReloadPlaceholderAlways:
    case PlaceholderOption::kShowAndDoNotReloadPlaceholder:
      return true;
    case PlaceholderOption::kReloadPlaceholderOnDecodeError:
    case PlaceholderOption::kDoNotReloadPlaceholder:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ImageResource::ShouldShowLazyImagePlaceholder() const {
  switch (placeholder_option_) {
    case PlaceholderOption::kShowAndReloadPlaceholderAlways:
    case PlaceholderOption::kShowAndDoNotReloadPlaceholder:
      return RuntimeEnabledFeatures::LazyImageLoadingEnabled() &&
             (GetResourceRequest().GetPreviewsState() &
              WebURLRequest::kLazyImageLoadDeferred);
    case PlaceholderOption::kReloadPlaceholderOnDecodeError:
    case PlaceholderOption::kDoNotReloadPlaceholder:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ImageResource::ShouldReloadBrokenPlaceholder() const {
  switch (placeholder_option_) {
    case PlaceholderOption::kShowAndReloadPlaceholderAlways:
      return ErrorOccurred();
    case PlaceholderOption::kReloadPlaceholderOnDecodeError:
      return GetStatus() == ResourceStatus::kDecodeError;
    case PlaceholderOption::kShowAndDoNotReloadPlaceholder:
    case PlaceholderOption::kDoNotReloadPlaceholder:
      return false;
  }
  NOTREACHED();
  return false;
}

void ImageResource::ReloadIfLoFiOrPlaceholderImage(
    ResourceFetcher* fetcher,
    ReloadLoFiOrPlaceholderPolicy policy) {
  if (policy == kReloadIfNeeded && !ShouldReloadBrokenPlaceholder())
    return;

  // Prevent clients and observers from being notified of completion while the
  // reload is being scheduled, so that e.g. canceling an existing load in
  // progress doesn't cause clients and observers to be notified of completion
  // prematurely.
  DCHECK(!is_scheduling_reload_);
  is_scheduling_reload_ = true;

  // The reloaded image should not use any previews transformations.
  WebURLRequest::PreviewsState previews_state_for_reload =
      WebURLRequest::kPreviewsNoTransform;

  SetPreviewsState(previews_state_for_reload);

  if (placeholder_option_ != PlaceholderOption::kDoNotReloadPlaceholder)
    ClearRangeRequestHeader();

    placeholder_option_ = PlaceholderOption::kDoNotReloadPlaceholder;

  if (IsLoading()) {
    Loader()->Cancel();
    // Canceling the loader causes error() to be called, which in turn calls
    // clear() and notifyObservers(), so there's no need to call these again
    // here.
  } else {
    ClearData();
    SetEncodedSize(0);
    UpdateImage(nullptr, ImageResourceContent::kClearImageAndNotifyObservers,
                false);
  }

  SetStatus(ResourceStatus::kNotStarted);

  DCHECK(is_scheduling_reload_);
  is_scheduling_reload_ = false;

  fetcher->StartLoad(this);
}

void ImageResource::OnePartInMultipartReceived(
    const ResourceResponse& response) {
  DCHECK(multipart_parser_);

  if (!GetResponse().IsNull()) {
    CHECK_EQ(GetResponse().WasFetchedViaServiceWorker(),
             response.WasFetchedViaServiceWorker());
    CHECK_EQ(GetResponse().GetType(), response.GetType());
  }

  SetResponse(response);
  if (multipart_parsing_state_ == MultipartParsingState::kWaitingForFirstPart) {
    // We have nothing to do because we don't have any data.
    multipart_parsing_state_ = MultipartParsingState::kParsingFirstPart;
    return;
  }
  UpdateImageAndClearBuffer();

  if (multipart_parsing_state_ == MultipartParsingState::kParsingFirstPart) {
    multipart_parsing_state_ = MultipartParsingState::kFinishedParsingFirstPart;
    // Notify finished when the first part ends.
    if (!ErrorOccurred())
      SetStatus(ResourceStatus::kCached);
    // We notify clients and observers of finish in checkNotify() and
    // updateImageAndClearBuffer(), respectively, and they will not be
    // notified again in Resource::finish()/error().
    NotifyFinished();
    if (Loader())
      Loader()->DidFinishLoadingFirstPartInMultipart();
  }
}

void ImageResource::MultipartDataReceived(const char* bytes, size_t size) {
  DCHECK(multipart_parser_);
  Resource::AppendData(bytes, size);
}

bool ImageResource::IsAccessAllowed(
    ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin
        does_current_frame_has_single_security_origin) const {
  if (does_current_frame_has_single_security_origin !=
      ImageResourceInfo::kHasSingleSecurityOrigin)
    return false;

  return GetResponse().IsCorsSameOrigin();
}

ImageResourceContent* ImageResource::GetContent() {
  return content_;
}

const ImageResourceContent* ImageResource::GetContent() const {
  return content_;
}

ResourcePriority ImageResource::PriorityFromObservers() {
  return GetContent()->PriorityFromObservers();
}

void ImageResource::UpdateImage(
    scoped_refptr<SharedBuffer> shared_buffer,
    ImageResourceContent::UpdateImageOption update_image_option,
    bool all_data_received) {
  bool is_multipart = !!multipart_parser_;
  auto result = GetContent()->UpdateImage(std::move(shared_buffer), GetStatus(),
                                          update_image_option,
                                          all_data_received, is_multipart);
  if (result == ImageResourceContent::UpdateImageResult::kShouldDecodeError) {
    // In case of decode error, we call imageNotifyFinished() iff we don't
    // initiate reloading:
    // [(a): when this is in the middle of loading, or (b): otherwise]
    // 1. The updateImage() call above doesn't call notifyObservers().
    // 2. notifyObservers(ShouldNotifyFinish) is called
    //    (a) via updateImage() called in ImageResource::finish()
    //        called via didFinishLoading() called in decodeError(), or
    //    (b) via updateImage() called in decodeError().
    //    imageNotifyFinished() is called here iff we will not initiate
    //    reloading in Step 3 due to notifyObservers()'s
    //    schedulingReloadOrShouldReloadBrokenPlaceholder() check.
    // 3. reloadIfLoFiOrPlaceholderImage() is called via ResourceFetcher
    //    (a) via didFinishLoading() called in decodeError(), or
    //    (b) after returning ImageResource::updateImage().
    DecodeError(all_data_received);
  }
}

void ImageResource::FlagAsUserAgentResource() {
  if (is_referenced_from_ua_stylesheet_)
    return;

  InstanceCounters::IncrementCounter(InstanceCounters::kUACSSResourceCounter);
  is_referenced_from_ua_stylesheet_ = true;
}

}  // namespace blink
