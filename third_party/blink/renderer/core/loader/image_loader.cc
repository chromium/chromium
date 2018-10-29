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

#include "third_party/blink/renderer/core/loader/image_loader.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

bool GetAbsoluteDimensionValue(const AtomicString& attribute_value,
                               double* value) {
  HTMLDimension dimension;
  if (ParseDimensionValue(attribute_value, dimension) &&
      dimension.IsAbsolute()) {
    *value = dimension.Value();
    return true;
  }
  return false;
}

bool IsLazyLoadingImageAllowed(const LocalFrame* frame,
                               HTMLImageElement* html_image) {
  // Minimum width or height attribute of the image to start lazyloading.
  const unsigned kMinDimensionToLazyLoad = 10;

  // Do not lazyload image elements created from javascript.
  if (!html_image->ElementCreatedByParser())
    return false;

  if (EqualIgnoringASCIICase(
          html_image->FastGetAttribute(HTMLNames::lazyloadAttr), "off") &&
      !frame->GetDocument()->IsLazyLoadPolicyEnforced()) {
    return false;
  }
  // Avoid lazyloading if width and height attributes are small. This
  // heuristic helps avoid double fetching tracking pixels.
  double width, height;
  if (GetAbsoluteDimensionValue(html_image->getAttribute(HTMLNames::widthAttr),
                                &width) &&
      GetAbsoluteDimensionValue(html_image->getAttribute(HTMLNames::heightAttr),
                                &height) &&
      width <= kMinDimensionToLazyLoad && height <= kMinDimensionToLazyLoad) {
    return false;
  }
  return frame->IsLazyLoadingImageAllowed();
}

}  // namespace

static ImageLoader::BypassMainWorldBehavior ShouldBypassMainWorldCSP(
    ImageLoader* loader) {
  DCHECK(loader);
  DCHECK(loader->GetElement());
  if (loader->GetElement()->GetDocument().GetFrame() &&
      loader->GetElement()
          ->GetDocument()
          .GetFrame()
          ->GetScriptController()
          .ShouldBypassMainWorldCSP())
    return ImageLoader::kBypassMainWorldCSP;
  return ImageLoader::kDoNotBypassMainWorldCSP;
}

class ImageLoader::Task {
 public:
  static std::unique_ptr<Task> Create(ImageLoader* loader,
                                      const KURL& request_url,
                                      UpdateFromElementBehavior update_behavior,
                                      ReferrerPolicy referrer_policy) {
    return std::make_unique<Task>(loader, request_url, update_behavior,
                                  referrer_policy);
  }

  Task(ImageLoader* loader,
       const KURL& request_url,
       UpdateFromElementBehavior update_behavior,
       ReferrerPolicy referrer_policy)
      : loader_(loader),
        should_bypass_main_world_csp_(ShouldBypassMainWorldCSP(loader)),
        update_behavior_(update_behavior),
        referrer_policy_(referrer_policy),
        request_url_(request_url),
        weak_factory_(this) {
    ExecutionContext& context = loader_->GetElement()->GetDocument();
    probe::AsyncTaskScheduled(&context, "Image", this);
    v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
    v8::HandleScope scope(isolate);
    // If we're invoked from C++ without a V8 context on the stack, we should
    // run the microtask in the context of the element's document's main world.
    if (!isolate->GetCurrentContext().IsEmpty()) {
      script_state_ = ScriptState::Current(isolate);
    } else {
      script_state_ = ToScriptStateForMainWorld(
          loader->GetElement()->GetDocument().GetFrame());
      DCHECK(script_state_);
    }
  }

  void Run() {
    if (!loader_)
      return;
    ExecutionContext& context = loader_->GetElement()->GetDocument();
    probe::AsyncTask async_task(&context, this);
    if (script_state_ && script_state_->ContextIsValid()) {
      ScriptState::Scope scope(script_state_);
      loader_->DoUpdateFromElement(should_bypass_main_world_csp_,
                                   update_behavior_, request_url_,
                                   referrer_policy_);
    } else {
      // This call does not access v8::Context internally.
      loader_->DoUpdateFromElement(should_bypass_main_world_csp_,
                                   update_behavior_, request_url_,
                                   referrer_policy_);
    }
  }

  void ClearLoader() {
    loader_ = nullptr;
    script_state_ = nullptr;
  }

  base::WeakPtr<Task> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  WeakPersistent<ImageLoader> loader_;
  BypassMainWorldBehavior should_bypass_main_world_csp_;
  UpdateFromElementBehavior update_behavior_;
  WeakPersistent<ScriptState> script_state_;
  ReferrerPolicy referrer_policy_;
  KURL request_url_;
  base::WeakPtrFactory<Task> weak_factory_;
};

ImageLoader::ImageLoader(Element* element)
    : element_(element),
      image_complete_(true),
      loading_image_document_(false),
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
    image_content_->RemoveObserver(this);
    image_content_ = nullptr;
    image_resource_for_image_document_ = nullptr;
    delay_until_image_notify_finished_ = nullptr;
  }
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
  for (auto& request : decode_requests_) {
    // If the image already in kDispatched state or still in kPEndingMicrotask
    // state, then we don't dispatch decodes for it. So, the only case to handle
    // is if we're in kPendingLoad state.
    if (request->state() != DecodeRequest::kPendingLoad)
      continue;
    Image* image = GetContent()->GetImage();

    // ImageLoader should be kept alive when decode is still pending. JS may
    // invoke 'decode' without capturing the Image object. If GC kicks in,
    // ImageLoader will be destroyed, leading to unresolved/unrejected Promise.
    frame->GetChromeClient().RequestDecode(
        frame, image->PaintImageForCurrentFrame(),
        WTF::Bind(&ImageLoader::DecodeRequestFinished,
                  WrapCrossThreadPersistent(this), request->request_id()));
    request->NotifyDecodeDispatched();
  }
}

void ImageLoader::DecodeRequestFinished(uint64_t request_id, bool success) {
  // First we find the corresponding request id, then we either resolve or
  // reject it and remove it from the list.
  for (auto* it = decode_requests_.begin(); it != decode_requests_.end();
       ++it) {
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
  for (auto* it = decode_requests_.begin(); it != decode_requests_.end();) {
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

void ImageLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_content_);
  visitor->Trace(image_resource_for_image_document_);
  visitor->Trace(element_);
  visitor->Trace(decode_requests_);
}

void ImageLoader::SetImageForTest(ImageResourceContent* new_image) {
  DCHECK(new_image);
  SetImageWithoutConsideringPendingLoadEvent(new_image);
}

bool ImageLoader::ShouldUpdateOnInsertedInto(
    ContainerNode& insertion_point) const {
  // If we're being inserted into a disconnected tree, we don't need to update.
  if (!insertion_point.isConnected())
    return false;

  // If the base element URL changed, it means that we might be in the process
  // of fetching a wrong image. We should update to ensure we fetch the correct
  // image. This can happen when inserting content into an iframe which has a
  // base element. See crbug.com/897545 for more details.
  if (element_->GetDocument().ValidBaseElementURL() != last_base_element_url_)
    return true;

  // Finally, try to update if we're idle (that is, we have neither the image
  // contents nor any activity). This could be an indication that we skipped a
  // previous load when inserted into an inactive document.
  return !image_content_ && !HasPendingActivity();
}

void ImageLoader::ClearImage() {
  SetImageWithoutConsideringPendingLoadEvent(nullptr);
}

void ImageLoader::SetImageForImageDocument(ImageResource* new_image_resource) {
  DCHECK(loading_image_document_);
  DCHECK(new_image_resource);
  DCHECK(new_image_resource->GetContent());

  image_resource_for_image_document_ = new_image_resource;
  SetImageWithoutConsideringPendingLoadEvent(new_image_resource->GetContent());

  // |image_complete_| is always true for ImageDocument loading, while the
  // loading is just started.
  // TODO(hiroshige): clean up the behavior of flags. https://crbug.com/719759
  image_complete_ = true;
}

void ImageLoader::SetImageWithoutConsideringPendingLoadEvent(
    ImageResourceContent* new_image_content) {
  DCHECK(failed_load_url_.IsEmpty());
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
    ImageLoader::BypassMainWorldBehavior bypass_behavior,
    Element& element,
    const ClientHintsPreferences& client_hints_preferences) {
  if (bypass_behavior == ImageLoader::kBypassMainWorldCSP)
    params.SetContentSecurityCheck(kDoNotCheckContentSecurityPolicy);

  CrossOriginAttributeValue cross_origin = GetCrossOriginAttributeValue(
      element.FastGetAttribute(HTMLNames::crossoriginAttr));
  if (cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(
        element.GetDocument().GetSecurityOrigin(), cross_origin);
  }

  if (RuntimeEnabledFeatures::PriorityHintsEnabled()) {
    mojom::FetchImportanceMode importance_mode =
        GetFetchImportanceAttributeValue(
            element.FastGetAttribute(HTMLNames::importanceAttr));
    params.SetFetchImportanceMode(importance_mode);
  }

  if (client_hints_preferences.ShouldSend(
          mojom::WebClientHintsType::kResourceWidth) &&
      IsHTMLImageElement(element))
    params.SetResourceWidth(ToHTMLImageElement(element).GetResourceWidth());
}

inline void ImageLoader::DispatchErrorEvent() {
  // There can be cases where DispatchErrorEvent() is called when there is
  // already a scheduled error event for the previous load attempt.
  // In such cases we cancel the previous event (by overwriting
  // |pending_error_event_|) and then re-schedule a new error event here.
  // crbug.com/722500
  pending_error_event_ = PostCancellableTask(
      *GetElement()->GetDocument().GetTaskRunner(TaskType::kDOMManipulation),
      FROM_HERE,
      WTF::Bind(&ImageLoader::DispatchPendingErrorEvent, WrapPersistent(this),
                WTF::Passed(IncrementLoadEventDelayCount::Create(
                    GetElement()->GetDocument()))));
}

inline void ImageLoader::CrossSiteOrCSPViolationOccurred(
    AtomicString image_source_url) {
  failed_load_url_ = image_source_url;
}

inline void ImageLoader::ClearFailedLoadURL() {
  failed_load_url_ = AtomicString();
}

inline void ImageLoader::EnqueueImageLoadingMicroTask(
    const KURL& request_url,
    UpdateFromElementBehavior update_behavior,
    ReferrerPolicy referrer_policy) {
  std::unique_ptr<Task> task =
      Task::Create(this, request_url, update_behavior, referrer_policy);
  pending_task_ = task->GetWeakPtr();
  Microtask::EnqueueMicrotask(
      WTF::Bind(&Task::Run, WTF::Passed(std::move(task))));
  delay_until_do_update_from_element_ =
      IncrementLoadEventDelayCount::Create(element_->GetDocument());
}

void ImageLoader::UpdateImageState(ImageResourceContent* new_image_content) {
  image_content_ = new_image_content;
  if (!new_image_content) {
    image_resource_for_image_document_ = nullptr;
    image_complete_ = true;
    if (lazy_image_load_state_ == LazyImageLoadState::kDeferred) {
      LazyLoadImageObserver::StopMonitoring(GetElement());
      lazy_image_load_state_ = LazyImageLoadState::kFullImage;
    }
  } else {
    image_complete_ = false;
    if (lazy_image_load_state_ == LazyImageLoadState::kDeferred)
      LazyLoadImageObserver::StartMonitoring(GetElement());
  }
  delay_until_image_notify_finished_ = nullptr;
}

void ImageLoader::DoUpdateFromElement(BypassMainWorldBehavior bypass_behavior,
                                      UpdateFromElementBehavior update_behavior,
                                      const KURL& url,
                                      ReferrerPolicy referrer_policy,
                                      UpdateType update_type) {
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
  if (!document.IsActive())
    return;

  AtomicString image_source_url = element_->ImageSourceURL();
  ImageResourceContent* new_image_content = nullptr;
  if (!url.IsNull() && !url.IsEmpty()) {
    // Unlike raw <img>, we block mixed content inside of <picture> or
    // <img srcset>.
    ResourceLoaderOptions resource_loader_options;
    resource_loader_options.initiator_info.name = GetElement()->localName();
    ResourceRequest resource_request(url);
    if (update_behavior == kUpdateForcedReload) {
      resource_request.SetCacheMode(mojom::FetchCacheMode::kBypassCache);
      resource_request.SetPreviewsState(WebURLRequest::kPreviewsNoTransform);
    }

    resource_request.SetReferrerPolicy(referrer_policy);

    // Correct the RequestContext if necessary.
    if (IsHTMLPictureElement(GetElement()->parentNode()) ||
        !GetElement()->FastGetAttribute(HTMLNames::srcsetAttr).IsNull()) {
      resource_request.SetRequestContext(mojom::RequestContextType::IMAGE_SET);
    } else if (IsHTMLObjectElement(GetElement())) {
      resource_request.SetRequestContext(mojom::RequestContextType::OBJECT);
    } else if (IsHTMLEmbedElement(GetElement())) {
      resource_request.SetRequestContext(mojom::RequestContextType::EMBED);
    }

    bool page_is_being_dismissed =
        document.PageDismissalEventBeingDispatched() != Document::kNoDismissal;
    if (page_is_being_dismissed) {
      resource_request.SetHTTPHeaderField(HTTPNames::Cache_Control,
                                          "max-age=0");
      resource_request.SetKeepalive(true);
      resource_request.SetRequestContext(mojom::RequestContextType::PING);
    }

    // Plug-ins should not load via service workers as plug-ins may have their
    // own origin checking logic that may get confused if service workers
    // respond with resources from another origin.
    // https://w3c.github.io/ServiceWorker/#implementer-concerns
    if (GetElement()->IsHTMLElement() &&
        ToHTMLElement(GetElement())->IsPluginElement()) {
      resource_request.SetSkipServiceWorker(true);
    }

    DCHECK(document.GetFrame());
    FetchParameters params(resource_request, resource_loader_options);
    ConfigureRequest(params, bypass_behavior, *element_,
                     document.GetFrame()->GetClientHintsPreferences());

    if (update_behavior != kUpdateForcedReload &&
        lazy_image_load_state_ == LazyImageLoadState::kNone) {
      const auto* frame = document.GetFrame();
      if (frame->IsClientLoFiAllowed(params.GetResourceRequest())) {
        params.SetClientLoFiPlaceholder();
      } else if (auto* html_image = ToHTMLImageElementOrNull(GetElement())) {
        if (IsLazyLoadingImageAllowed(frame, html_image)) {
          params.SetLazyImagePlaceholder();
          lazy_image_load_state_ = LazyImageLoadState::kDeferred;
        }
        LazyLoadImageObserver::StartTrackingVisibilityMetrics(html_image);
      }
    }

    new_image_content = ImageResourceContent::Fetch(params, document.Fetcher());

    // If this load is starting while navigating away, treat it as an auditing
    // keepalive request, and don't report its results back to the element.
    if (page_is_being_dismissed)
      new_image_content = nullptr;

    ClearFailedLoadURL();
  } else {
    if (!image_source_url.IsNull()) {
      // Fire an error event if the url string is not empty, but the KURL is.
      DispatchErrorEvent();
    }
    NoImageResourceToLoad();
  }

  ImageResourceContent* old_image_content = image_content_.Get();
  if (old_image_content != new_image_content)
    RejectPendingDecodes(update_type);

  if (update_behavior == kUpdateSizeChanged && element_->GetLayoutObject() &&
      element_->GetLayoutObject()->IsImage() &&
      new_image_content == old_image_content) {
    ToLayoutImage(element_->GetLayoutObject())->IntrinsicSizeChanged();
  } else {
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
                                    ReferrerPolicy referrer_policy) {
  AtomicString image_source_url = element_->ImageSourceURL();
  suppress_error_events_ = (update_behavior == kUpdateSizeChanged);
  last_base_element_url_ =
      element_->GetDocument().ValidBaseElementURL().GetString();

  if (update_behavior == kUpdateIgnorePreviousError)
    ClearFailedLoadURL();

  if (!failed_load_url_.IsEmpty() && image_source_url == failed_load_url_)
    return;

  if (loading_image_document_ && update_behavior == kUpdateForcedReload) {
    // Prepares for reloading ImageDocument.
    // We turn the ImageLoader into non-ImageDocument here, and proceed to
    // reloading just like an ordinary <img> element below.
    loading_image_document_ = false;
    image_resource_for_image_document_ = nullptr;
    ClearImage();
  }

  KURL url = ImageSourceToKURL(image_source_url);

  // Prevent the creation of a ResourceLoader (and therefore a network request)
  // for ImageDocument loads. In this case, the image contents have already been
  // requested as a main resource and ImageDocumentParser will take care of
  // funneling the main resource bytes into |image_content_|, so just create an
  // ImageResource to be populated later.
  if (loading_image_document_) {
    ResourceRequest request(url);
    request.SetFetchCredentialsMode(
        network::mojom::FetchCredentialsMode::kOmit);
    ImageResource* image_resource = ImageResource::Create(request);
    image_resource->NotifyStartLoad();
    SetImageForImageDocument(image_resource);
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

  if (ShouldLoadImmediately(url)) {
    DoUpdateFromElement(kDoNotBypassMainWorldCSP, update_behavior, url,
                        referrer_policy, UpdateType::kSync);
    return;
  }
  // Allow the idiom "img.src=''; img.src='.." to clear down the image before an
  // asynchronous load completes.
  if (image_source_url.IsEmpty()) {
    ImageResourceContent* image = image_content_.Get();
    if (image) {
      image->RemoveObserver(this);
    }
    image_content_ = nullptr;
    image_resource_for_image_document_ = nullptr;
    delay_until_image_notify_finished_ = nullptr;
    if (lazy_image_load_state_ != LazyImageLoadState::kNone) {
      LazyLoadImageObserver::StopMonitoring(GetElement());
      lazy_image_load_state_ = LazyImageLoadState::kNone;
    }
  }

  // Don't load images for inactive documents. We don't want to slow down the
  // raw HTML parsing case by loading images we don't intend to display.
  Document& document = element_->GetDocument();
  if (document.IsActive())
    EnqueueImageLoadingMicroTask(url, update_behavior, referrer_policy);
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
    if (!stripped_image_source_url.IsEmpty())
      url = document.CompleteURL(stripped_image_source_url);
  }
  return url;
}

bool ImageLoader::ShouldLoadImmediately(const KURL& url) const {
  // We force any image loads which might require alt content through the
  // asynchronous path so that we can add the shadow DOM for the alt-text
  // content when style recalc is over and DOM mutation is allowed again.
  if (!url.IsNull()) {
    Resource* resource = GetMemoryCache()->ResourceForURL(
        url, element_->GetDocument().Fetcher()->GetCacheIdentifier());
    if (resource && !resource->ErrorOccurred())
      return true;
  }
  return (IsHTMLObjectElement(element_) || IsHTMLEmbedElement(element_));
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
      IncrementLoadEventDelayCount::Create(document);
}

void ImageLoader::ImageNotifyFinished(ImageResourceContent* resource) {
  RESOURCE_LOADING_DVLOG(1)
      << "ImageLoader::imageNotifyFinished " << this
      << "; has pending load event=" << pending_load_event_.IsActive();

  DCHECK(failed_load_url_.IsEmpty());
  DCHECK_EQ(resource, image_content_.Get());

  // |image_complete_| is always true for entire ImageDocument loading for
  // historical reason.
  // DoUpdateFromElement() is not called and SetImageForImageDocument()
  // is called instead for ImageDocument loading.
  // TODO(hiroshige): Turn the CHECK()s to DCHECK()s before going to beta.
  if (loading_image_document_)
    CHECK(image_complete_);
  else
    CHECK(!image_complete_);

  if (lazy_image_load_state_ == LazyImageLoadState::kDeferred) {
    // LazyImages: if a placeholder is loaded, suppress load events and do not
    // consider the image as loaded, except for unblocking document load events.
    // The final image load (including load events) occurs when the
    // non-placeholder image loading (triggered by LoadDeferredImage()) is
    // finished.
    if (image_content_ && image_content_->GetImage()->IsPlaceholderImage()) {
      delay_until_image_notify_finished_ = nullptr;
      return;
    }
    // A placeholder was requested, but the result was an error or a full image.
    // In these cases, consider this as the final image and suppress further
    // reloading and proceed to the image load completion process below.
    LazyLoadImageObserver::StopMonitoring(GetElement());
    lazy_image_load_state_ = LazyImageLoadState::kFullImage;
  }

  image_complete_ = true;
  delay_until_image_notify_finished_ = nullptr;

  // Update ImageAnimationPolicy for image_content_.
  if (image_content_)
    image_content_->UpdateImageAnimationPolicy();

  UpdateLayoutObject();

  if (image_content_ && image_content_->HasImage()) {
    Image& image = *image_content_->GetImage();

    if (image.IsSVGImage()) {
      SVGImage& svg_image = ToSVGImage(image);
      // SVG's document should be completely loaded before access control
      // checks, which can occur anytime after ImageNotifyFinished()
      // (See SVGImage::CurrentFrameHasSingleSecurityOrigin()).
      // We check the document is loaded here to catch violation of the
      // assumption reliably.
      svg_image.CheckLoaded();
      svg_image.UpdateUseCounters(GetElement()->GetDocument());
    }
  }

  DispatchDecodeRequestsIfComplete();

  if (auto* html_image = ToHTMLImageElementOrNull(GetElement()))
    LazyLoadImageObserver::RecordMetricsOnLoadFinished(html_image);

  if (loading_image_document_) {
    CHECK(!pending_load_event_.IsActive());
    return;
  }

  if (resource->ErrorOccurred()) {
    pending_load_event_.Cancel();

    base::Optional<ResourceError> error = resource->GetResourceError();
    if (error && error->IsAccessCheck())
      CrossSiteOrCSPViolationOccurred(AtomicString(error->FailingURL()));

    // The error event should not fire if the image data update is a result of
    // environment change.
    // https://html.spec.whatwg.org/multipage/embedded-content.html#the-img-element:the-img-element-55
    if (!suppress_error_events_)
      DispatchErrorEvent();
    return;
  }

  CHECK(!pending_load_event_.IsActive());
  pending_load_event_ = PostCancellableTask(
      *GetElement()->GetDocument().GetTaskRunner(TaskType::kDOMManipulation),
      FROM_HERE,
      WTF::Bind(&ImageLoader::DispatchPendingLoadEvent, WrapPersistent(this),
                WTF::Passed(IncrementLoadEventDelayCount::Create(
                    GetElement()->GetDocument()))));
}

LayoutImageResource* ImageLoader::GetLayoutImageResource() {
  LayoutObject* layout_object = element_->GetLayoutObject();

  if (!layout_object)
    return nullptr;

  // We don't return style generated image because it doesn't belong to the
  // ImageLoader. See <https://bugs.webkit.org/show_bug.cgi?id=42840>
  if (layout_object->IsImage() &&
      !ToLayoutImage(layout_object)->IsGeneratedContent())
    return ToLayoutImage(layout_object)->ImageResource();

  if (layout_object->IsSVGImage())
    return ToLayoutSVGImage(layout_object)->ImageResource();

  if (layout_object->IsVideo())
    return ToLayoutVideo(layout_object)->ImageResource();

  return nullptr;
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

bool ImageLoader::HasPendingEvent() const {
  // Regular image loading is in progress.
  if (image_content_ && !image_complete_ && !loading_image_document_)
    return true;

  if (pending_load_event_.IsActive() || pending_error_event_.IsActive())
    return true;

  return false;
}

void ImageLoader::DispatchPendingLoadEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  if (!image_content_)
    return;
  CHECK(image_complete_);
  if (GetElement()->GetDocument().GetFrame())
    DispatchLoadEvent();

  // Checks Document's load event synchronously here for performance.
  // This is safe because DispatchPendingLoadEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

void ImageLoader::DispatchPendingErrorEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  if (GetElement()->GetDocument().GetFrame())
    GetElement()->DispatchEvent(*Event::Create(EventTypeNames::error));

  // Checks Document's load event synchronously here for performance.
  // This is safe because DispatchPendingErrorEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

bool ImageLoader::GetImageAnimationPolicy(ImageAnimationPolicy& policy) {
  if (!GetElement()->GetDocument().GetSettings())
    return false;

  policy = GetElement()->GetDocument().GetSettings()->GetImageAnimationPolicy();
  return true;
}

ScriptPromise ImageLoader::Decode(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  // It's possible that |script_state|'s context isn't valid, which means we
  // should immediately reject the request. This is possible in situations like
  // the document that created this image was already destroyed (like an img
  // that comes from iframe.contentDocument.createElement("img") and the iframe
  // is destroyed).
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                      "The source image cannot be decoded.");
    return ScriptPromise();
  }

  UseCounter::Count(GetElement()->GetDocument(), WebFeature::kImageDecodeAPI);

  auto* request =
      new DecodeRequest(this, ScriptPromiseResolver::Create(script_state));
  Microtask::EnqueueMicrotask(
      WTF::Bind(&DecodeRequest::ProcessForTask, WrapWeakPersistent(request)));
  decode_requests_.push_back(request);
  return request->promise();
}

void ImageLoader::LoadDeferredImage(ReferrerPolicy referrer_policy) {
  if (lazy_image_load_state_ != LazyImageLoadState::kDeferred)
    return;
  DCHECK(!image_complete_);
  lazy_image_load_state_ = LazyImageLoadState::kFullImage;
  UpdateFromElement(kUpdateNormal, referrer_policy);
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

ImageLoader::DecodeRequest::DecodeRequest(ImageLoader* loader,
                                          ScriptPromiseResolver* resolver)
    : request_id_(s_next_request_id_++), resolver_(resolver), loader_(loader) {}

void ImageLoader::DecodeRequest::Resolve() {
  resolver_->Resolve();
  loader_ = nullptr;
}

void ImageLoader::DecodeRequest::Reject() {
  resolver_->Reject(DOMException::Create(
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

void ImageLoader::DecodeRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  visitor->Trace(loader_);
}

}  // namespace blink
