/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/loader/image_loader.h"

#include <memory>
#include <utility>

#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/lazy_image_helper.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// This implements the HTML Standard's list of available images tuple-matching
// logic [1]. In our implementation, it is only used to determine whether or not
// we should skip queueing the microtask that continues the rest of the image
// loading algorithm. But the actual decision to reuse the image is determined
// by ResourceFetcher, and is much stricter.
// [1]:
// https://html.spec.whatwg.org/multipage/images.html#updating-the-image-data:list-of-available-images
bool CanReuseFromListOfAvailableImages(
    const Resource* resource,
    CrossOriginAttributeValue cross_origin_attribute,
    const SecurityOrigin* origin) {
  const ResourceRequestHead& request = resource->GetResourceRequest();
  bool is_same_origin = request.RequestorOrigin()->IsSameOriginWith(origin);
  if (cross_origin_attribute != kCrossOriginAttributeNotSet && !is_same_origin)
    return false;

  if (request.GetCredentialsMode() ==
          network::mojom::CredentialsMode::kSameOrigin &&
      cross_origin_attribute != kCrossOriginAttributeAnonymous) {
    return false;
  }

  return true;
}

}  // namespace

class ImageLoader::Task {
 public:
  Task(ImageLoader* loader, UpdateFromElementBehavior update_behavior)
      : loader_(loader), update_behavior_(update_behavior) {
    ExecutionContext* context = loader_->GetElement()->GetExecutionContext();
    async_task_context_.Schedule(context, "Image");
    world_ = context->GetCurrentWorld();
  }

  void Run() {
    if (!loader_)
      return;
    ExecutionContext* context = loader_->GetElement()->GetExecutionContext();
    probe::AsyncTask async_task(context, &async_task_context_);
    loader_->DoUpdateFromElement(world_.Get(), update_behavior_);
  }

  void ClearLoader() {
    loader_ = nullptr;
    world_ = nullptr;
  }

  base::WeakPtr<Task> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  WeakPersistent<ImageLoader> loader_;
  UpdateFromElementBehavior update_behavior_;
  Persistent<const DOMWrapperWorld> world_;

  probe::AsyncTaskContext async_task_context_;
  base::WeakPtrFactory<Task> weak_factory_{this};
};

ImageLoader::ImageLoader(Element* element)
    : element_(element),
      image_complete_(true),
      suppress_error_events_(false),
      lazy_image_load_state_(LazyImageLoadState::kNone) {
  RESOURCE_LOADING_DVLOG(1) << "new ImageLoader " << this;
}

ImageLoader::~ImageLoader() = default;

void ImageLoader::Dispose() {
  RESOURCE_LOADING_DVLOG(1)
      << "~ImageLoader " << this
      << "; has pending load event=" << pending_load_event_.IsActive()
      << ", has pending error event=" << pending_error_event_.IsActive();

  if (image_content_) {
    delay_until_image_notify_finished_ = nullptr;
  }
}

static bool ImageTypeNeedsDecode(const Image& image) {
  // SVG images are context sensitive, and decoding them without the proper
  // context will just end up wasting memory (and CPU).
  // TODO(vmpstr): Generalize this to be all non-lazy decoded images.
  if (IsA<SVGImage>(image))
    return false;
  return true;
}

void ImageLoader::DispatchDecodeRequestsIfComplete() {
  // If the current image isn't complete, then we can't dispatch any decodes.
  // This function will be called again when the current image completes.
  if (!image_complete_)
    return;

  bool is_active = GetElement()->GetDocument().IsActive();
  // If any of the following conditions hold, we either have an inactive
  // document or a broken/non-existent image. In those cases, we reject any
  // pending decodes.
  if (!is_active || !GetContent() || GetContent()->ErrorOccurred()) {
    RejectPendingDecodes();
    return;
  }

  LocalFrame* frame = GetElement()->GetDocument().GetFrame();
  auto it = decode_requests_.begin();
  while (it != decode_requests_.end()) {
    // If the image already in kDispatched state or still in kPendingMicrotask
    // state, then we don't dispatch decodes for it. So, the only case to handle
    // is if we're in kPendingLoad state.
    auto& request = *it;
    if (request->state() != DecodeRequest::kPendingLoad) {
      ++it;
      continue;
    }
    Image* image = GetContent()->GetImage();
    if (!ImageTypeNeedsDecode(*image)) {
      // If the image is of a type that doesn't need decode, resolve the
      // promise.
      request->Resolve();
      it = decode_requests_.erase(it);
      continue;
    }
    // ImageLoader should be kept alive when decode is still pending. JS may
    // invoke 'decode' without capturing the Image object. If GC kicks in,
    // ImageLoader will be destroyed, leading to unresolved/unrejected Promise.
    frame->GetChromeClient().RequestDecode(
        frame, image->PaintImageForCurrentFrame(),
        WTF::BindOnce(&ImageLoader::DecodeRequestFinished,
                      MakeUnwrappingCrossThreadHandle(this),
                      request->request_id()));
    request->NotifyDecodeDispatched();
    ++it;
  }
}

void ImageLoader::DecodeRequestFinished(uint64_t request_id, bool success) {
  // First we find the corresponding request id, then we either resolve or
  // reject it and remove it from the list.
  for (auto it = decode_requests_.begin(); it != decode_requests_.end(); ++it) {
    auto& request = *it;
    if (request->request_id() != request_id)
      continue;

    if (success)
      request->Resolve();
    else
      request->Reject();
    decode_requests_.erase(it);
    break;
  }
}

void ImageLoader::RejectPendingDecodes(UpdateType update_type) {
  // Normally, we only reject pending decodes that have passed the
  // kPendingMicrotask state, since pending mutation requests still have an
  // outstanding microtask that will run and might act on a different image than
  // the current one. However, as an optimization, there are cases where we
  // synchronously update the image (see UpdateFromElement). In those cases, we
  // have to reject even the pending mutation requests because conceptually they
  // would have been scheduled before the synchronous update ran, so they
  // referred to the old image.
  for (auto it = decode_requests_.begin(); it != decode_requests_.end();) {
    auto& request = *it;
    if (update_type == UpdateType::kAsync &&
        request->state() == DecodeRequest::kPendingMicrotask) {
      ++it;
      continue;
    }
    request->Reject();
    it = decode_requests_.erase(it);
  }
}

void ImageLoader::Trace(Visitor* visitor) const {
  visitor->Trace(image_content_);
  visitor->Trace(image_content_for_image_document_);
  visitor->Trace(element_);
  visitor->Trace(decode_requests_);
  ImageResourceObserver::Trace(visitor);
}

void ImageLoader::SetImageForTest(ImageResourceContent* new_image) {
  DCHECK(new_image);
  SetImageWithoutConsideringPendingLoadEvent(new_image);
}

bool ImageLoader::ImageIsPotentiallyAvailable() const {
  bool is_lazyload = lazy_image_load_state_ == LazyImageLoadState::kDeferred;

  bool image_has_loaded = image_content_ && !image_content_->IsLoading() &&
                          !image_content_->ErrorOccurred();
  bool image_still_loading = !image_has_loaded && HasPendingActivity() &&
                             !HasPendingError() &&
                             !element_->ImageSourceURL().empty();
  bool image_has_image = image_content_ && image_content_->HasImage();
  bool image_is_document = element_->GetDocument().IsImageDocument() &&
                           image_content_ && !image_content_->ErrorOccurred();

  // Icky special case for deferred images:
  // A deferred image is not loading, does have pending activity, does not
  // have an error, but it does have an ImageResourceContent associated
  // with it, so |image_has_loaded| will be true even though the image hasn't
  // actually loaded. Fixing the definition of |image_has_loaded| isn't
  // sufficient, because a deferred image does have pending activity, does not
  // have a pending error, and does have a source URL, so if |image_has_loaded|
  // was correct, |image_still_loading| would become wrong.
  //
  // Instead of dealing with that, there's a separate check that the
  // ImageResourceContent has non-null image data associated with it, which
  // isn't folded into |image_has_loaded| above.
  return (image_has_loaded && image_has_image) || image_still_loading ||
         image_is_document || is_lazyload;
}

void ImageLoader::ClearImage() {
  SetImageWithoutConsideringPendingLoadEvent(nullptr);
}

void ImageLoader::SetImageWithoutConsideringPendingLoadEvent(
    ImageResourceContent* new_image_content) {
  DCHECK(failed_load_url_.empty());
  ImageResourceContent* old_image_content = image_content_.Get();
  if (new_image_content != old_image_content) {
    if (pending_load_event_.IsActive())
      pending_load_event_.Cancel();
    if (pending_error_event_.IsActive())
      pending_error_event_.Cancel();
    UpdateImageState(new_image_content);
    if (new_image_content) {
      new_image_content->AddObserver(this);
    }
    if (old_image_content) {
      old_image_content->RemoveObserver(this);
    }
  }

  if (LayoutImageResource* image_resource = GetLayoutImageResource())
    image_resource->ResetAnimation();
}

static void ConfigureRequest(
    FetchParameters& params,
    Element& element,
    const ClientHintsPreferences& client_hints_preferences) {
  CrossOriginAttributeValue cross_origin = GetCrossOriginAttributeValue(
      element.FastGetAttribute(html_names::kCrossoriginAttr));
  if (cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(
        element.GetExecutionContext()->GetSecurityOrigin(), cross_origin);
  }

  mojom::blink::FetchPriorityHint fetch_priority_hint =
      GetFetchPriorityAttributeValue(
          element.FastGetAttribute(html_names::kFetchpriorityAttr));
  params.SetFetchPriorityHint(fetch_priority_hint);

  auto* html_image_element = DynamicTo<HTMLImageElement>(element);
  if ((client_hints_preferences.ShouldSend(
           network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED) ||
       client_hints_preferences.ShouldSend(
           network::mojom::WebClientHintsType::kResourceWidth)) &&
      html_image_element) {
    params.SetResourceWidth(html_image_element->GetResourceWidth());
  }
}

inline void ImageLoader::QueuePendingErrorEvent() {
  // The error event should not fire if the image data update is a result of
  // environment change.
  // https://html.spec.whatwg.org/C/#the-img-element:the-img-element-55
  if (suppress_error_events_) {
    return;
  }
  // There can be cases where QueuePendingErrorEvent() is called when there
  // is already a scheduled error event for the previous load attempt.
  // In such cases we cancel the previous event (by overwriting
  // |pending_error_event_|) and then re-schedule a new error event here.
  // crbug.com/722500
  pending_error_event_ = PostCancellableTask(
      *GetElement()->GetDocument().GetTaskRunner(TaskType::kDOMManipulation),
      FROM_HERE,
      WTF::BindOnce(&ImageLoader::DispatchPendingErrorEvent,
                    WrapPersistent(this),
                    std::make_unique<IncrementLoadEventDelayCount>(
                        GetElement()->GetDocument())));
}

inline void ImageLoader::CrossSiteOrCSPViolationOccurred(
    AtomicString image_source_url) {
  failed_load_url_ = image_source_url;
}

inline void ImageLoader::ClearFailedLoadURL() {
  failed_load_url_ = AtomicString();
}

inline void ImageLoader::EnqueueImageLoadingMicroTask(
    UpdateFromElementBehavior update_behavior) {
  auto task = std::make_unique<Task>(this, update_behavior);
  pending_task_ = task->GetWeakPtr();
  element_->GetDocument().GetAgent().event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&Task::Run, std::move(task)));
  delay_until_do_update_from_element_ =
      std::make_unique<IncrementLoadEventDelayCount>(element_->GetDocument());
}

void ImageLoader::UpdateImageState(ImageResourceContent* new_image_content) {
  image_content_ = new_image_content;
  if (!new_image_content) {
    image_content_for_image_document_ = nullptr;
    image_complete_ = true;
    if (lazy_image_load_state_ == LazyImageLoadState::kDeferred) {
      LazyImageHelper::StopMonitoring(GetElement());
      lazy_image_load_state_ = LazyImageLoadState::kNone;
    }
  } else {
    image_complete_ = false;
    if (lazy_image_load_state_ == LazyImageLoadState::kDeferred)
      LazyImageHelper::StartMonitoring(GetElement());
  }
  delay_until_image_notify_finished_ = nullptr;
}

void ImageLoader::DoUpdateFromElement(const DOMWrapperWorld* world,
                                      UpdateFromElementBehavior update_behavior,
                                      UpdateType update_type,
                                      bool force_blocking) {
  // FIXME: According to
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/embedded-content.html#the-img-element:the-img-element-55
  // When "update image" is called due to environment changes and the load
  // fails, onerror should not be called. That is currently not the case.
  //
  // We don't need to call clearLoader here: Either we were called from the
  // task, or our caller updateFromElement cleared the task's loader (and set
  // pending_task_ to null).
  pending_task_.reset();
  // Make sure to only decrement the count when we exit this function
  std::unique_ptr<IncrementLoadEventDelayCount> load_delay_counter;
  load_delay_counter.swap(delay_until_do_update_from_element_);

  Document& document = element_->GetDocument();
  if (!document.IsActive()) {
    // Clear if the loader was moved into a not fully active document - or the
    // document was detached - after the microtask was queued. If moved into a
    // not fully active document, ElementDidMoveToNewDocument() will have
    // called ClearImage() already, but in the case of a detached document it
    // won't have.
    ClearImage();
    return;
  }

  AtomicString image_source_url = element_->ImageSourceURL();
  const KURL url = ImageSourceToKURL(image_source_url);
  ImageResourceContent* new_image_content = nullptr;
  if (!url.IsNull() && !url.IsEmpty()) {
    // Unlike raw <img>, we block mixed content inside of <picture> or
    // <img srcset>.
    ResourceLoaderOptions resource_loader_options(std::move(world));
    resource_loader_options.initiator_info.name = GetElement()->localName();
    ResourceRequest resource_request(url);
    if (update_behavior == kUpdateForcedReload) {
      resource_request.SetCacheMode(mojom::blink::FetchCacheMode::kBypassCache);
    }

    network::mojom::ReferrerPolicy referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    AtomicString referrer_policy_attribute =
        element_->FastGetAttribute(html_names::kReferrerpolicyAttr);
    if (!referrer_policy_attribute.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          referrer_policy_attribute, kSupportReferrerPolicyLegacyKeywords,
          &referrer_policy);
    }
    resource_request.SetReferrerPolicy(referrer_policy);

    // Correct the RequestContext if necessary.
    if (IsA<HTMLPictureElement>(GetElement()->parentNode()) ||
        !GetElement()->FastGetAttribute(html_names::kSrcsetAttr).IsNull()) {
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::IMAGE_SET);
      resource_request.SetRequestDestination(
          network::mojom::RequestDestination::kImage);
    } else if (IsA<HTMLObjectElement>(GetElement())) {
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::OBJECT);
      resource_request.SetRequestDestination(
          network::mojom::RequestDestination::kObject);
    } else if (IsA<HTMLEmbedElement>(GetElement())) {
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::EMBED);
      resource_request.SetRequestDestination(
          network::mojom::RequestDestination::kEmbed);
    }

    DCHECK(document.GetFrame());
    auto* frame = document.GetFrame();

    if (IsA<HTMLImageElement>(GetElement())) {
      if (GetElement()->FastHasAttribute(html_names::kAttributionsrcAttr) &&
          frame->GetAttributionSrcLoader()->CanRegister(
              url, To<HTMLImageElement>(GetElement()),
              /*request_id=*/std::nullopt)) {
        resource_request.SetAttributionReportingEligibility(
            network::mojom::AttributionReportingEligibility::
                kEventSourceOrTrigger);
      }
      bool shared_storage_writable_opted_in =
          GetElement()->FastHasAttribute(
              html_names::kSharedstoragewritableAttr) &&
          RuntimeEnabledFeatures::SharedStorageAPIM118Enabled(
              GetElement()->GetExecutionContext()) &&
          GetElement()->GetExecutionContext()->IsSecureContext() &&
          !SecurityOrigin::Create(url)->IsOpaque();
      resource_request.SetSharedStorageWritableOptedIn(
          shared_storage_writable_opted_in);
    }

    bool page_is_being_dismissed =
        document.PageDismissalEventBeingDispatched() != Document::kNoDismissal;
    if (page_is_being_dismissed) {
      resource_request.SetHttpHeaderField(http_names::kCacheControl,
                                          AtomicString("max-age=0"));
      resource_request.SetKeepalive(true);
      resource_request.SetRequestContext(
          mojom::blink::RequestContextType::PING);
      UseCounter::Count(document, WebFeature::kImageLoadAtDismissalEvent);
    }

    // Plug-ins should not load via service workers as plug-ins may have their
    // own origin checking logic that may get confused if service workers
    // respond with resources from another origin.
    // https://w3c.github.io/ServiceWorker/#implementer-concerns
    auto* html_element = DynamicTo<HTMLElement>(GetElement());
    if (html_element && html_element->IsPluginElement()) {
      resource_request.SetSkipServiceWorker(true);
    }

    FetchParameters params(std::move(resource_request),
                           resource_loader_options);

    ConfigureRequest(params, *element_, frame->GetClientHintsPreferences());

    if (update_behavior != kUpdateForcedReload &&
        lazy_image_load_state_ != LazyImageLoadState::kFullImage) {
      if (auto* html_image = DynamicTo<HTMLImageElement>(GetElement())) {
        if (LazyImageHelper::ShouldDeferImageLoad(*frame, html_image)) {
          lazy_image_load_state_ = LazyImageLoadState::kDeferred;
          params.SetLazyImageDeferred();
        }
      }
    }

    // If we're now loading in a once-deferred image, make sure it doesn't
    // block the load event.
    if (lazy_image_load_state_ == LazyImageLoadState::kFullImage &&
        !force_blocking) {
      params.SetLazyImageNonBlocking();
    }

    new_image_content = ImageResourceContent::Fetch(params, document.Fetcher());

    // If this load is starting while navigating away, treat it as an auditing
    // keepalive request, and don't report its results back to the element.
    if (page_is_being_dismissed) {
      new_image_content = nullptr;
    }

    ClearFailedLoadURL();
  } else {
    if (!image_source_url.IsNull()) {
      // Fire an error event if the url string is not empty, but the KURL is.
      QueuePendingErrorEvent();
    }
    NoImageResourceToLoad();
  }

  ImageResourceContent* old_image_content = image_content_.Get();
  if (old_image_content != new_image_content)
    RejectPendingDecodes(update_type);

  if (update_behavior == kUpdateSizeChanged && element_->GetLayoutObject() &&
      element_->GetLayoutObject()->IsImage() &&
      new_image_content == old_image_content) {
    To<LayoutImage>(element_->GetLayoutObject())->IntrinsicSizeChanged();
  } else {
    bool is_lazyload = lazy_image_load_state_ == LazyImageLoadState::kDeferred;

    // Loading didn't start (loading of images was disabled). We show fallback
    // contents here, while we don't dispatch an 'error' event etc., because
    // spec-wise the image remains in the "Unavailable" state.
    if (new_image_content &&
        new_image_content->GetContentStatus() == ResourceStatus::kNotStarted &&
        !is_lazyload)
      NoImageResourceToLoad();

    if (pending_load_event_.IsActive())
      pending_load_event_.Cancel();

    // Cancel error events that belong to the previous load, which is now
    // cancelled by changing the src attribute. If newImage is null and
    // has_pending_error_event_ is true, we know the error event has been just
    // posted by this load and we should not cancel the event.
    // FIXME: If both previous load and this one got blocked with an error, we
    // can receive one error event instead of two.
    if (pending_error_event_.IsActive() && new_image_content)
      pending_error_event_.Cancel();

    UpdateImageState(new_image_content);

    UpdateLayoutObject();
    // If newImage exists and is cached, addObserver() will result in the load
    // event being queued to fire. Ensure this happens after beforeload is
    // dispatched.
    if (new_image_content) {
      new_image_content->AddObserver(this);
    }
    if (old_image_content) {
      old_image_content->RemoveObserver(this);
    }
  }

  if (LayoutImageResource* image_resource = GetLayoutImageResource())
    image_resource->ResetAnimation();
}

void ImageLoader::UpdateFromElement(UpdateFromElementBehavior update_behavior,
                                    bool force_blocking) {
  if (!element_->GetDocument().IsActive()) {
    return;
  }

  AtomicString image_source_url = element_->ImageSourceURL();
  suppress_error_events_ = (update_behavior == kUpdateSizeChanged);

  if (update_behavior == kUpdateIgnorePreviousError)
    ClearFailedLoadURL();

  if (!failed_load_url_.empty() && image_source_url == failed_load_url_)
    return;

  // Prevent the creation of a ResourceLoader (and therefore a network request)
  // for ImageDocument loads. In this case, the image contents have already been
  // requested as a main resource and ImageDocumentParser will take care of
  // funneling the main resource bytes into |image_content_for_image_document_|,
  // so just pick up the ImageResourceContent that has been provided.
  if (image_content_for_image_document_) {
    DCHECK_NE(update_behavior, kUpdateForcedReload);
    SetImageWithoutConsideringPendingLoadEvent(
        image_content_for_image_document_);
    image_content_for_image_document_ = nullptr;
    return;
  }

  // If we have a pending task, we have to clear it -- either we're now loading
  // immediately, or we need to reset the task's state.
  if (pending_task_) {
    pending_task_->ClearLoader();
    pending_task_.reset();
    // Here we need to clear delay_until_do_update_from_element to avoid causing
    // a memory leak in case it's already created.
    delay_until_do_update_from_element_ = nullptr;
  }

  if (ShouldLoadImmediately(ImageSourceToKURL(image_source_url)) &&
      update_behavior != kUpdateFromMicrotask) {
    DoUpdateFromElement(element_->GetExecutionContext()->GetCurrentWorld(),
                        update_behavior, UpdateType::kSync, force_blocking);
    return;
  }
  // Allow the idiom "img.src=''; img.src='.." to clear down the image before an
  // asynchronous load completes.
  if (image_source_url.empty()) {
    ImageResourceContent* image = image_content_.Get();
    if (image) {
      image->RemoveObserver(this);
    }
    image_content_ = nullptr;
    image_complete_ = true;
    image_content_for_image_document_ = nullptr;
    delay_until_image_notify_finished_ = nullptr;
    if (lazy_image_load_state_ != LazyImageLoadState::kNone) {
      LazyImageHelper::StopMonitoring(GetElement());
      lazy_image_load_state_ = LazyImageLoadState::kNone;
    }
  } else {
    image_complete_ = false;
  }

  // Don't load images for inactive documents or active documents without V8
  // context. We don't want to slow down the raw HTML parsing case by loading
  // images we don't intend to display.
  if (element_->GetDocument().IsActive())
    EnqueueImageLoadingMicroTask(update_behavior);
}

KURL ImageLoader::ImageSourceToKURL(AtomicString image_source_url) const {
  KURL url;

  // Don't load images for inactive documents. We don't want to slow down the
  // raw HTML parsing case by loading images we don't intend to display.
  Document& document = element_->GetDocument();
  if (!document.IsActive())
    return url;

  // Do not load any image if the 'src' attribute is missing or if it is
  // an empty string.
  if (!image_source_url.IsNull()) {
    String stripped_image_source_url =
        StripLeadingAndTrailingHTMLSpaces(image_source_url);
    if (!stripped_image_source_url.empty())
      url = document.CompleteURL(stripped_image_source_url);
  }
  return url;
}

bool ImageLoader::ShouldLoadImmediately(const KURL& url) const {
  // We force any image loads which might require alt content through the
  // asynchronous path so that we can add the shadow DOM for the alt-text
  // content when style recalc is over and DOM mutation is allowed again.
  if (!url.IsNull()) {
    Resource* resource = MemoryCache::Get()->ResourceForURL(
        url, element_->GetDocument().Fetcher()->GetCacheIdentifier(url));

    if (resource && !resource->ErrorOccurred() &&
        CanReuseFromListOfAvailableImages(
            resource,
            GetCrossOriginAttributeValue(
                element_->FastGetAttribute(html_names::kCrossoriginAttr)),
            element_->GetExecutionContext()->GetSecurityOrigin())) {
      return true;
    }
  }

  return (IsA<HTMLObjectElement>(*element_) ||
          IsA<HTMLEmbedElement>(*element_) || IsA<HTMLVideoElement>(*element_));
}

void ImageLoader::ImageChanged(ImageResourceContent* content,
                               CanDeferInvalidation) {
  DCHECK_EQ(content, image_content_.Get());
  if (image_complete_ || !content->IsLoading() ||
      delay_until_image_notify_finished_)
    return;

  Document& document = element_->GetDocument();
  if (!document.IsActive())
    return;

  delay_until_image_notify_finished_ =
      std::make_unique<IncrementLoadEventDelayCount>(document);
}

void ImageLoader::ImageNotifyFinished(ImageResourceContent* content) {
  RESOURCE_LOADING_DVLOG(1)
      << "ImageLoader::imageNotifyFinished " << this
      << "; has pending load event=" << pending_load_event_.IsActive();

  DCHECK(failed_load_url_.empty());
  DCHECK_EQ(content, image_content_.Get());

  CHECK(!image_complete_);

  if (lazy_image_load_state_ == LazyImageLoadState::kDeferred) {
    // A placeholder was requested, but the result was an error or a full image.
    // In these cases, consider this as the final image and suppress further
    // reloading and proceed to the image load completion process below.
    LazyImageHelper::StopMonitoring(GetElement());
    lazy_image_load_state_ = LazyImageLoadState::kFullImage;
  }

  image_complete_ = true;
  delay_until_image_notify_finished_ = nullptr;

  UpdateLayoutObject();

  if (image_content_ && image_content_->HasImage()) {
    Image& image = *image_content_->GetImage();

    if (auto* svg_image = DynamicTo<SVGImage>(image)) {
      // Check that the SVGImage has completed loading (i.e the 'load' event
      // has been dispatched in the SVG document).
      svg_image->CheckLoaded();
      svg_image->UpdateUseCounters(GetElement()->GetDocument());
      svg_image->MaybeRecordSvgImageProcessingTime(GetElement()->GetDocument());
    }
  }


  DispatchDecodeRequestsIfComplete();

  if (content->ErrorOccurred()) {
    pending_load_event_.Cancel();

    std::optional<ResourceError> error = content->GetResourceError();
    if (error && error->IsAccessCheck())
      CrossSiteOrCSPViolationOccurred(AtomicString(error->FailingURL()));

    QueuePendingErrorEvent();
    return;
  }

  content->RecordDecodedImageType(&element_->GetDocument());

  CHECK(!pending_load_event_.IsActive());
  pending_load_event_ = PostCancellableTask(
      *GetElement()->GetDocument().GetTaskRunner(TaskType::kDOMManipulation),
      FROM_HERE,
      WTF::BindOnce(&ImageLoader::DispatchPendingLoadEvent,
                    WrapPersistent(this),
                    std::make_unique<IncrementLoadEventDelayCount>(
                        GetElement()->GetDocument())));
}

LayoutImageResource* ImageLoader::GetLayoutImageResource() const {
  LayoutObject* layout_object = element_->GetLayoutObject();

  if (!layout_object)
    return nullptr;

  // We don't return style generated image because it doesn't belong to the
  // ImageLoader. See <https://bugs.webkit.org/show_bug.cgi?id=42840>
  if (layout_object->IsImage() &&
      !To<LayoutImage>(layout_object)->IsGeneratedContent())
    return To<LayoutImage>(layout_object)->ImageResource();

  if (layout_object->IsSVGImage())
    return To<LayoutSVGImage>(layout_object)->ImageResource();

  if (auto* layout_video = DynamicTo<LayoutVideo>(layout_object))
    return layout_video->ImageResource();

  return nullptr;
}

void ImageLoader::OnAttachLayoutTree() {
  LayoutImageResource* image_resource = GetLayoutImageResource();
  if (!image_resource) {
    return;
  }
  // If the LayoutImageResource already has an image, it either means that it
  // hasn't been freshly created or that it is generated content ("content:
  // url(...)") - in which case we don't need to do anything or shouldn't do
  // anything respectively.
  if (image_resource->HasImage()) {
    return;
  }
  image_resource->SetImageResource(image_content_);
}

void ImageLoader::UpdateLayoutObject() {
  LayoutImageResource* image_resource = GetLayoutImageResource();

  if (!image_resource)
    return;

  // Only update the layoutObject if it doesn't have an image or if what we have
  // is a complete image.  This prevents flickering in the case where a dynamic
  // change is happening between two images.
  ImageResourceContent* cached_image_content = image_resource->CachedImage();
  if (image_content_ != cached_image_content &&
      (image_complete_ || !cached_image_content))
    image_resource->SetImageResource(image_content_.Get());
}

ResourcePriority ImageLoader::ComputeResourcePriority() const {
  LayoutImageResource* image_resource = GetLayoutImageResource();
  if (!image_resource)
    return ResourcePriority();

  ResourcePriority priority = image_resource->ComputeResourcePriority();
  priority.source = ResourcePriority::Source::kImageLoader;

  static const bool is_image_lcpp_enabled =
      base::FeatureList::IsEnabled(features::kLCPCriticalPathPredictor) &&
      features::
          kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement
              .Get();
  if (is_image_lcpp_enabled) {
    if (auto* html_image_element =
            DynamicTo<HTMLImageElement>(element_.Get())) {
      priority.is_lcp_resource = html_image_element->IsPredictedLcpElement();
    }
  }
  return priority;
}

bool ImageLoader::HasPendingEvent() const {
  // Regular image loading is in progress.
  if (image_content_ && !image_complete_ &&
      lazy_image_load_state_ != LazyImageLoadState::kDeferred) {
    return true;
  }

  if (pending_load_event_.IsActive() || pending_error_event_.IsActive() ||
      !decode_requests_.empty()) {
    return true;
  }

  return false;
}

void ImageLoader::DispatchPendingLoadEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  if (!image_content_)
    return;
  CHECK(image_complete_);
  DispatchLoadEvent();

  // Checks Document's load event synchronously here for performance.
  // This is safe because DispatchPendingLoadEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

void ImageLoader::DispatchPendingErrorEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  DispatchErrorEvent();

  // Checks Document's load event synchronously here for performance.
  // This is safe because DispatchPendingErrorEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

bool ImageLoader::GetImageAnimationPolicy(
    mojom::blink::ImageAnimationPolicy& policy) {
  if (!GetElement()->GetDocument().GetSettings())
    return false;

  policy = GetElement()->GetDocument().GetSettings()->GetImageAnimationPolicy();
  return true;
}

ScriptPromise<IDLUndefined> ImageLoader::Decode(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // It's possible that |script_state|'s context isn't valid, which means we
  // should immediately reject the request. This is possible in situations like
  // the document that created this image was already destroyed (like an img
  // that comes from iframe.contentDocument.createElement("img") and the iframe
  // is destroyed).
  if (!script_state->ContextIsValid() || !execution_context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                      "The source image cannot be decoded.");
    return EmptyPromise();
  }

  UseCounter::Count(execution_context, WebFeature::kImageDecodeAPI);

  auto* request = MakeGarbageCollected<DecodeRequest>(
      this, MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
                script_state, exception_state.GetContext()));
  execution_context->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      &DecodeRequest::ProcessForTask, WrapWeakPersistent(request)));
  decode_requests_.push_back(request);
  return request->promise();
}

void ImageLoader::LoadDeferredImage(bool force_blocking,
                                    bool update_from_microtask) {
  if (lazy_image_load_state_ != LazyImageLoadState::kDeferred)
    return;
  DCHECK(!image_complete_);
  lazy_image_load_state_ = LazyImageLoadState::kFullImage;

  // If the image has been fully deferred (no placeholder fetch), report it as
  // fully loaded now.
  UpdateFromElement(
      update_from_microtask ? kUpdateFromMicrotask : kUpdateNormal,
      force_blocking);
}

void ImageLoader::ElementDidMoveToNewDocument() {
  if (delay_until_do_update_from_element_) {
    delay_until_do_update_from_element_->DocumentChanged(
        element_->GetDocument());
  }
  if (delay_until_image_notify_finished_) {
    delay_until_image_notify_finished_->DocumentChanged(
        element_->GetDocument());
  }
  ClearFailedLoadURL();
  ClearImage();
}

// Indicates the next available id that we can use to uniquely identify a decode
// request.
uint64_t ImageLoader::DecodeRequest::s_next_request_id_ = 0;

ImageLoader::DecodeRequest::DecodeRequest(
    ImageLoader* loader,
    ScriptPromiseResolver<IDLUndefined>* resolver)
    : request_id_(s_next_request_id_++), resolver_(resolver), loader_(loader) {}

void ImageLoader::DecodeRequest::Resolve() {
  resolver_->Resolve();
  loader_ = nullptr;
}

void ImageLoader::DecodeRequest::Reject() {
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kEncodingError, "The source image cannot be decoded."));
  loader_ = nullptr;
}

void ImageLoader::DecodeRequest::ProcessForTask() {
  // We could have already processed (ie rejected) this task due to a sync
  // update in UpdateFromElement. In that case, there's nothing to do here.
  if (!loader_)
    return;

  DCHECK_EQ(state_, kPendingMicrotask);
  state_ = kPendingLoad;
  loader_->DispatchDecodeRequestsIfComplete();
}

void ImageLoader::DecodeRequest::NotifyDecodeDispatched() {
  DCHECK_EQ(state_, kPendingLoad);
  state_ = kDispatched;
}

void ImageLoader::DecodeRequest::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(loader_);
}

}  // namespace blink
