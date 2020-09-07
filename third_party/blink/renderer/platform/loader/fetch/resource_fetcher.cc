/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/auto_reset.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/stale_revalidation_resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

constexpr uint32_t ResourceFetcher::kKeepaliveInflightBytesQuota;

namespace {

constexpr base::TimeDelta kKeepaliveLoadersTimeout =
    base::TimeDelta::FromSeconds(30);

// Timeout for link preloads to be used after window.onload
static constexpr base::TimeDelta kUnusedPreloadTimeout =
    base::TimeDelta::FromSeconds(3);

#define RESOURCE_HISTOGRAM_PREFIX "Blink.MemoryCache.RevalidationPolicy."

#define DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, name)                         \
  case ResourceType::k##name: {                                                \
    UMA_HISTOGRAM_ENUMERATION(RESOURCE_HISTOGRAM_PREFIX prefix #name, policy); \
    break;                                                                     \
  }

#define DEFINE_RESOURCE_HISTOGRAM(prefix)                    \
  switch (factory.GetType()) {                               \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, CSSStyleSheet)  \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Font)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Image)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, ImportResource) \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, LinkPrefetch)   \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Manifest)       \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Audio)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Video)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Mock)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Raw)            \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Script)         \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, SVGDocument)    \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, TextTrack)      \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, XSLStyleSheet)  \
  }

ResourceLoadPriority TypeToPriority(ResourceType type) {
  switch (type) {
    case ResourceType::kCSSStyleSheet:
    case ResourceType::kFont:
      // Also parser-blocking scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kVeryHigh;
    case ResourceType::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
      FALLTHROUGH;
    case ResourceType::kRaw:
    case ResourceType::kImportResource:
    case ResourceType::kScript:
      // Also visible resources/images (set explicitly in loadPriority)
      return ResourceLoadPriority::kHigh;
    case ResourceType::kManifest:
    case ResourceType::kMock:
      // Also late-body scripts and stylesheets discovered by the
      // preload scanner (set explicitly in loadPriority)
      return ResourceLoadPriority::kMedium;
    case ResourceType::kImage:
    case ResourceType::kTextTrack:
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kSVGDocument:
      // Also async scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kLow;
    case ResourceType::kLinkPrefetch:
      return ResourceLoadPriority::kVeryLow;
  }

  NOTREACHED();
  return ResourceLoadPriority::kUnresolved;
}

bool ShouldResourceBeAddedToMemoryCache(const FetchParameters& params,
                                        Resource* resource) {
  return IsMainThread() &&
         params.GetResourceRequest().HttpMethod() == http_names::kGET &&
         params.Options().data_buffering_policy != kDoNotBufferData &&
         !IsRawResource(*resource);
}

static ResourceFetcher::ResourceFetcherSet& MainThreadFetchersSet() {
  DEFINE_STATIC_LOCAL(
      Persistent<ResourceFetcher::ResourceFetcherSet>, fetchers,
      (MakeGarbageCollected<ResourceFetcher::ResourceFetcherSet>()));
  return *fetchers;
}

static bool& PriorityObserverMapCreated() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(bool, priority_observer_map_created, (false));
  return priority_observer_map_created;
}

// Calls to PriorityObservers() that don't need to explicitly interact with the
// map should be guarded with a call to PriorityObserverMapCreated(), to avoid
// unnecessarily creating a PriorityObserverMap.
using PriorityObserverMap = HashMap<String, base::OnceCallback<void(int)>>;
static ThreadSpecific<PriorityObserverMap>& PriorityObservers() {
  PriorityObserverMapCreated() = true;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<PriorityObserverMap>, map, ());
  return map;
}

ResourceLoadPriority AdjustPriorityWithPriorityHint(
    ResourceLoadPriority priority_so_far,
    ResourceType type,
    const ResourceRequestHead& resource_request,
    FetchParameters::DeferOption defer_option,
    bool is_link_preload) {
  mojom::FetchImportanceMode importance_mode =
      resource_request.GetFetchImportanceMode();

  ResourceLoadPriority new_priority = priority_so_far;

  switch (importance_mode) {
    case mojom::FetchImportanceMode::kImportanceAuto:
      break;
    case mojom::FetchImportanceMode::kImportanceHigh:
      // Boost priority of
      // - Late and async scripts
      // - Images
      // - Prefetch
      if ((type == ResourceType::kScript &&
           (FetchParameters::kLazyLoad == defer_option)) ||
          type == ResourceType::kImage || type == ResourceType::kLinkPrefetch) {
        new_priority = ResourceLoadPriority::kHigh;
      }

      DCHECK_LE(priority_so_far, new_priority);
      break;
    case mojom::FetchImportanceMode::kImportanceLow:
      // Demote priority of:
      // - Images
      //     Note: this will only have a real effect on in-viewport images since
      //     out-of-viewport images already have priority set to kLow
      // - Link preloads
      //     For this initial implementation we do a blanket demotion regardless
      //     of `as` value/type.
      if (type == ResourceType::kImage ||
          resource_request.GetRequestContext() ==
              mojom::RequestContextType::FETCH ||
          is_link_preload) {
        new_priority = ResourceLoadPriority::kLow;
      }

      DCHECK_LE(new_priority, priority_so_far);
      break;
  }

  return new_priority;
}

ResourceLoadPriority AdjustPriorityWithDeferScriptIntervention(
    const FetchContext& fetch_context,
    ResourceLoadPriority priority_so_far,
    ResourceType type,
    const ResourceRequestHead& resource_request,
    FetchParameters::DeferOption defer_option,
    bool is_link_preload) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kLowerJavaScriptPriorityWhenForceDeferred)) {
    return priority_so_far;
  }

  PreviewsState context_previews_state = fetch_context.previews_state();

  if (type != ResourceType::kScript)
    return priority_so_far;

  // If none of the JavaScript resources are render blocking (due to the
  // DeferAllScript intervention), then lower their priority so they do not
  // contend for network resources with higher priority resources that may be
  // render blocking (e.g., html, css). ResourceLoadPriority::kMedium
  // corresponds to a network priority of
  // network::mojom::blink::RequestPriority::kLow which is considered delayable
  // by the resource scheduler on the browser side.
  if (RuntimeEnabledFeatures::ForceDeferScriptInterventionEnabled() ||
      (context_previews_state & PreviewsTypes::kDeferAllScriptOn)) {
    return std::min(priority_so_far, ResourceLoadPriority::kMedium);
  }
  return priority_so_far;
}

std::unique_ptr<TracedValue> BeginResourceLoadData(
    const blink::ResourceRequest& request) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", request.Url().GetString());
  return value;
}

std::unique_ptr<TracedValue> EndResourceLoadFailData() {
  auto value = std::make_unique<TracedValue>();
  value->SetString("outcome", "Fail");
  return value;
}

std::unique_ptr<TracedValue> ResourcePrioritySetData(
    blink::ResourceLoadPriority priority) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("priority", static_cast<int>(priority));
  return value;
}

// This function corresponds with step 2 substep 7 of
// https://fetch.spec.whatwg.org/#main-fetch.
void SetReferrer(ResourceRequest& request,
                 const FetchClientSettingsObject& fetch_client_settings_object,
                 UseCounter& use_counter) {
  String referrer_to_use = request.ReferrerString();
  network::mojom::ReferrerPolicy referrer_policy_to_use =
      request.GetReferrerPolicy();

  if (referrer_to_use == Referrer::ClientReferrerString())
    referrer_to_use = fetch_client_settings_object.GetOutgoingReferrer();

  bool used_referrer_policy_from_context = false;
  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault) {
    used_referrer_policy_from_context = true;
    referrer_policy_to_use = fetch_client_settings_object.GetReferrerPolicy();
  }

  Referrer generated_referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, request.Url(), referrer_to_use);

  // The request's referrer would be different in the absence of <meta
  // name=referrer> tags with comma-separated-list values exactly when both
  // 1. the request falls back to its client settings object's policy; and
  // 2. if we recompute the referrer using the value of the client settings
  // object's policy, disregarding any policy that came from a meta tag with a
  // comma-separated value, we obtain a different referrer.
  if (used_referrer_policy_from_context) {
    base::Optional<network::mojom::blink::ReferrerPolicy>
        policy_but_for_meta_tags_with_policy_lists =
            fetch_client_settings_object
                .GetReferrerPolicyDisregardingMetaTagsContainingLists();

    bool referrer_would_be_different_absent_meta_tags_with_policy_lists =
        policy_but_for_meta_tags_with_policy_lists.has_value() &&
        (generated_referrer.referrer !=
         SecurityPolicy::GenerateReferrer(
             *policy_but_for_meta_tags_with_policy_lists, request.Url(),
             referrer_to_use)
             .referrer);

    if (referrer_would_be_different_absent_meta_tags_with_policy_lists) {
      use_counter.CountUse(
          mojom::WebFeature::
              kHTMLMetaElementReferrerPolicyMultipleTokensAffectingRequest);
    }
  }

  request.SetReferrerString(generated_referrer.referrer);
  request.SetReferrerPolicy(generated_referrer.referrer_policy);
}

void PopulateAndAddResourceTimingInfo(Resource* resource,
                                      scoped_refptr<ResourceTimingInfo> info,
                                      base::TimeTicks response_end,
                                      int64_t encoded_data_length) {
  info->SetInitialURL(
      resource->GetResourceRequest().GetRedirectInfo().has_value()
          ? resource->GetResourceRequest().GetRedirectInfo()->original_url
          : resource->GetResourceRequest().Url());
  info->SetFinalResponse(resource->GetResponse());
  info->SetLoadResponseEnd(response_end);
  // encodedDataLength == -1 means "not available".
  // TODO(ricea): Find cases where it is not available but the
  // PerformanceResourceTiming spec requires it to be available and fix
  // them.
  info->AddFinalTransferSize(encoded_data_length == -1 ? 0
                                                       : encoded_data_length);
}

}  // namespace

ResourceFetcherInit::ResourceFetcherInit(
    DetachableResourceFetcherProperties& properties,
    FetchContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ResourceFetcher::LoaderFactory* loader_factory,
    ContextLifecycleNotifier* context_lifecycle_notifier)
    : properties(&properties),
      context(context),
      task_runner(std::move(task_runner)),
      loader_factory(loader_factory),
      context_lifecycle_notifier(context_lifecycle_notifier) {
  DCHECK(context);
  DCHECK(this->task_runner);
  DCHECK(loader_factory || properties.IsDetached());
  DCHECK(context_lifecycle_notifier || properties.IsDetached());
}

mojom::RequestContextType ResourceFetcher::DetermineRequestContext(
    ResourceType type,
    IsImageSet is_image_set) {
  DCHECK((is_image_set == kImageNotImageSet) ||
         (type == ResourceType::kImage && is_image_set == kImageIsImageSet));
  switch (type) {
    case ResourceType::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
      FALLTHROUGH;
    case ResourceType::kCSSStyleSheet:
      return mojom::RequestContextType::STYLE;
    case ResourceType::kScript:
      return mojom::RequestContextType::SCRIPT;
    case ResourceType::kFont:
      return mojom::RequestContextType::FONT;
    case ResourceType::kImage:
      if (is_image_set == kImageIsImageSet)
        return mojom::RequestContextType::IMAGE_SET;
      return mojom::RequestContextType::IMAGE;
    case ResourceType::kRaw:
      return mojom::RequestContextType::SUBRESOURCE;
    case ResourceType::kImportResource:
      return mojom::RequestContextType::IMPORT;
    case ResourceType::kLinkPrefetch:
      return mojom::RequestContextType::PREFETCH;
    case ResourceType::kTextTrack:
      return mojom::RequestContextType::TRACK;
    case ResourceType::kSVGDocument:
      return mojom::RequestContextType::IMAGE;
    case ResourceType::kAudio:
      return mojom::RequestContextType::AUDIO;
    case ResourceType::kVideo:
      return mojom::RequestContextType::VIDEO;
    case ResourceType::kManifest:
      return mojom::RequestContextType::MANIFEST;
    case ResourceType::kMock:
      return mojom::RequestContextType::SUBRESOURCE;
  }
  NOTREACHED();
  return mojom::RequestContextType::SUBRESOURCE;
}

network::mojom::RequestDestination ResourceFetcher::DetermineRequestDestination(
    ResourceType type) {
  switch (type) {
    case ResourceType::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
      FALLTHROUGH;
    case ResourceType::kCSSStyleSheet:
      return network::mojom::RequestDestination::kStyle;
    case ResourceType::kScript:
      return network::mojom::RequestDestination::kScript;
    case ResourceType::kFont:
      return network::mojom::RequestDestination::kFont;
    case ResourceType::kImage:
      return network::mojom::RequestDestination::kImage;
    case ResourceType::kTextTrack:
      return network::mojom::RequestDestination::kTrack;
    case ResourceType::kSVGDocument:
      return network::mojom::RequestDestination::kImage;
    case ResourceType::kAudio:
      return network::mojom::RequestDestination::kAudio;
    case ResourceType::kVideo:
      return network::mojom::RequestDestination::kVideo;
    case ResourceType::kManifest:
      return network::mojom::RequestDestination::kManifest;
    case ResourceType::kRaw:
    case ResourceType::kImportResource:
    case ResourceType::kLinkPrefetch:
    case ResourceType::kMock:
      return network::mojom::RequestDestination::kEmpty;
  }
  NOTREACHED();
  return network::mojom::RequestDestination::kEmpty;
}

// static
void ResourceFetcher::AddPriorityObserverForTesting(
    const KURL& resource_url,
    base::OnceCallback<void(int)> callback) {
  PriorityObservers()->Set(
      MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url),
      std::move(callback));
}

// This method simply takes in information about a ResourceRequest, and returns
// a priority. It will not be called for ResourceRequests that already have a
// pre-set priority (e.g., requests coming from a Service Worker) except for
// images, which may need to be reprioritized.
ResourceLoadPriority ResourceFetcher::ComputeLoadPriority(
    ResourceType type,
    const ResourceRequestHead& resource_request,
    ResourcePriority::VisibilityStatus visibility,
    FetchParameters::DeferOption defer_option,
    FetchParameters::SpeculativePreloadType speculative_preload_type,
    bool is_link_preload) {
  DCHECK(!resource_request.PriorityHasBeenSet() ||
         type == ResourceType::kImage);
  ResourceLoadPriority priority = TypeToPriority(type);

  // Visible resources (images in practice) get a boost to High priority.
  if (visibility == ResourcePriority::kVisible)
    priority = ResourceLoadPriority::kHigh;

  // Resources before the first image are considered "early" in the document and
  // resources after the first image are "late" in the document.  Important to
  // note that this is based on when the preload scanner discovers a resource
  // for the most part so the main parser may not have reached the image element
  // yet.
  if (type == ResourceType::kImage && !is_link_preload)
    image_fetched_ = true;

  // Check for late-in-document resources discovered by the preload scanner.
  // kInDocument means it was found in the document by the preload scanner.
  // image_fetched_ is used as the divider between "early" and "late" where
  // anything after the first image is considered "late" in the document.
  // This is used for lowering the priority of late-body scripts/stylesheets.
  bool late_document_from_preload_scanner = false;
  if (speculative_preload_type ==
          FetchParameters::SpeculativePreloadType::kInDocument &&
      image_fetched_) {
    late_document_from_preload_scanner = true;
  }

  // A preloaded font should not take precedence over critical CSS or
  // parser-blocking scripts.
  if (type == ResourceType::kFont && is_link_preload)
    priority = ResourceLoadPriority::kHigh;

  if (FetchParameters::kIdleLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (type == ResourceType::kScript) {
    // Special handling for scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in
    // typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (FetchParameters::kLazyLoad == defer_option) {
      priority = ResourceLoadPriority::kLow;
    } else if (late_document_from_preload_scanner) {
      priority = ResourceLoadPriority::kMedium;
    }
  } else if (type == ResourceType::kCSSStyleSheet &&
             late_document_from_preload_scanner) {
    // Lower the priority of late-body stylesheets discovered by the preload
    // scanner. They do not block render and this gives them the same behavior
    // as late-body scripts. If the main parser reaches the stylesheet before
    // it is loaded, a non-speculative fetch will be made and the priority will
    // be boosted (just like with scripts).
    priority = ResourceLoadPriority::kMedium;
  } else if (FetchParameters::kLazyLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (resource_request.GetRequestContext() ==
                 mojom::RequestContextType::BEACON ||
             resource_request.GetRequestContext() ==
                 mojom::RequestContextType::PING ||
             resource_request.GetRequestContext() ==
                 mojom::RequestContextType::CSP_REPORT) {
    if (base::FeatureList::IsEnabled(features::kSetLowPriorityForBeacon)) {
      priority = ResourceLoadPriority::kLow;
    } else {
      priority = ResourceLoadPriority::kVeryLow;
    }
  }

  priority = AdjustPriorityWithPriorityHint(priority, type, resource_request,
                                            defer_option, is_link_preload);

  priority = AdjustPriorityWithDeferScriptIntervention(
      Context(), priority, type, resource_request, defer_option,
      is_link_preload);

  if (properties_->IsSubframeDeprioritizationEnabled()) {
    if (properties_->IsMainFrame()) {
      UMA_HISTOGRAM_ENUMERATION(
          "LowPriorityIframes.MainFrameRequestPriority", priority,
          static_cast<int>(ResourceLoadPriority::kHighest) + 1);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "LowPriorityIframes.IframeRequestPriority", priority,
          static_cast<int>(ResourceLoadPriority::kHighest) + 1);
      // When enabled, the priority of all resources in subframe is dropped.
      // Non-delayable resources are assigned a priority of kLow, and the rest
      // of them are assigned a priority of kLowest. This ensures that if the
      // webpage fetches most of its primary content using iframes, then high
      // priority requests within the iframe go on the network first.
      if (priority >= ResourceLoadPriority::kHigh) {
        priority = ResourceLoadPriority::kLow;
      } else {
        priority = ResourceLoadPriority::kLowest;
      }
    }
  }

  return priority;
}

ResourceFetcher::ResourceFetcher(const ResourceFetcherInit& init)
    : properties_(*init.properties),
      context_(init.context),
      task_runner_(init.task_runner),
      use_counter_(init.use_counter
                       ? init.use_counter
                       : MakeGarbageCollected<DetachableUseCounter>(nullptr)),
      console_logger_(init.console_logger
                          ? init.console_logger
                          : MakeGarbageCollected<DetachableConsoleLogger>()),
      loader_factory_(init.loader_factory),
      scheduler_(MakeGarbageCollected<ResourceLoadScheduler>(
          init.initial_throttling_policy,
          init.throttle_option_override,
          *properties_,
          init.frame_or_worker_scheduler,
          *console_logger_)),
      archive_(init.archive),
      resource_timing_report_timer_(
          task_runner_,
          this,
          &ResourceFetcher::ResourceTimingReportTimerFired),
      frame_or_worker_scheduler_(
          init.frame_or_worker_scheduler
              ? init.frame_or_worker_scheduler->GetWeakPtr()
              : nullptr),
      blob_registry_remote_(init.context_lifecycle_notifier),
      auto_load_images_(true),
      images_enabled_(true),
      allow_stale_resources_(false),
      image_fetched_(false),
      should_log_request_as_invalid_in_imported_document_(false) {
  InstanceCounters::IncrementCounter(InstanceCounters::kResourceFetcherCounter);
  if (IsMainThread())
    MainThreadFetchersSet().insert(this);
}

ResourceFetcher::~ResourceFetcher() {
  InstanceCounters::DecrementCounter(InstanceCounters::kResourceFetcherCounter);
}

bool ResourceFetcher::IsDetached() const {
  return properties_->IsDetached();
}

Resource* ResourceFetcher::CachedResource(const KURL& resource_url) const {
  if (resource_url.IsEmpty())
    return nullptr;
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);
  const WeakMember<Resource>& resource = cached_resources_map_.at(url);
  return resource.Get();
}

mojom::ControllerServiceWorkerMode
ResourceFetcher::IsControlledByServiceWorker() const {
  return properties_->GetControllerServiceWorkerMode();
}

bool ResourceFetcher::ResourceNeedsLoad(Resource* resource,
                                        const FetchParameters& params,
                                        RevalidationPolicy policy) {
  // Defer a font load until it is actually needed unless this is a link
  // preload.
  if (resource->GetType() == ResourceType::kFont && !params.IsLinkPreload())
    return false;

  // Defer loading images either when:
  // - images are disabled
  // - instructed to defer loading images from network
  if (resource->GetType() == ResourceType::kImage &&
      (ShouldDeferImageLoad(resource->Url()) ||
       params.GetImageRequestBehavior() == FetchParameters::kDeferImageLoad)) {
    return false;
  }
  return policy != RevalidationPolicy::kUse || resource->StillNeedsLoad();
}

void ResourceFetcher::DidLoadResourceFromMemoryCache(
    Resource* resource,
    const ResourceRequest& request,
    bool is_static_data) {
  if (IsDetached() || !resource_load_observer_)
    return;

  resource_load_observer_->WillSendRequest(
      request.InspectorId(), request, ResourceResponse() /* redirects */,
      resource->GetType(), resource->Options().initiator_info);
  resource_load_observer_->DidReceiveResponse(
      request.InspectorId(), request, resource->GetResponse(), resource,
      ResourceLoadObserver::ResponseSource::kFromMemoryCache);
  if (resource->EncodedSize() > 0) {
    resource_load_observer_->DidReceiveData(
        request.InspectorId(),
        base::span<const char>(nullptr, resource->EncodedSize()));
  }

  resource_load_observer_->DidFinishLoading(
      request.InspectorId(), base::TimeTicks(), 0,
      resource->GetResponse().DecodedBodyLength(), false);

  if (!is_static_data) {
    // Resources loaded from memory cache should be reported the first time
    // they're used.
    scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
        resource->Options().initiator_info.name, base::TimeTicks::Now(),
        request.GetRequestContext(), request.GetRequestDestination());
    // TODO(yoav): Getting the original URL before redirects here is only needed
    // until Out-of-Blink CORS lands: https://crbug.com/736308
    info->SetInitialURL(
        resource->GetResourceRequest().GetRedirectInfo().has_value()
            ? resource->GetResourceRequest().GetRedirectInfo()->original_url
            : resource->GetResourceRequest().Url());
    ResourceResponse final_response = resource->GetResponse();
    final_response.SetResourceLoadTiming(nullptr);
    info->SetFinalResponse(final_response);
    info->SetLoadResponseEnd(info->InitialTime());
    scheduled_resource_timing_reports_.push_back(std::move(info));
    if (!resource_timing_report_timer_.IsActive())
      resource_timing_report_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  }
}

Resource* ResourceFetcher::ResourceForStaticData(
    const FetchParameters& params,
    const ResourceFactory& factory) {
  const KURL& url = params.GetResourceRequest().Url();
  DCHECK(url.ProtocolIsData() || archive_);

  if (!archive_ && factory.GetType() == ResourceType::kRaw)
    return nullptr;

  const String cache_identifier = GetCacheIdentifier(url);
  // Most off-main-thread resource fetches use Resource::kRaw and don't reach
  // this point, but off-main-thread module fetches might.
  if (IsMainThread()) {
    if (Resource* old_resource =
            GetMemoryCache()->ResourceForURL(url, cache_identifier)) {
      // There's no reason to re-parse if we saved the data from the previous
      // parse.
      if (params.Options().data_buffering_policy != kDoNotBufferData)
        return old_resource;
      GetMemoryCache()->Remove(old_resource);
    }
  }

  ResourceResponse response;
  scoped_refptr<SharedBuffer> data;
  if (url.ProtocolIsData()) {
    int result;
    std::tie(result, response, data) = network_utils::ParseDataURL(
        url, params.GetResourceRequest().HttpMethod());
    if (result != net::OK) {
      return nullptr;
    }
    // TODO(yhirano): Consider removing this.
    if (!IsSupportedMimeType(response.MimeType().Utf8())) {
      return nullptr;
    }
  } else {
    ArchiveResource* archive_resource =
        archive_->SubresourceForURL(params.Url());
    // The archive doesn't contain the resource, the request must be aborted.
    if (!archive_resource)
      return nullptr;
    data = archive_resource->Data();
    response.SetCurrentRequestUrl(url);
    response.SetMimeType(archive_resource->MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(archive_resource->TextEncoding());
    response.SetFromArchive(true);
  }

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->NotifyStartLoad();
  // FIXME: We should provide a body stream here.
  resource->ResponseReceived(response);
  resource->SetDataBufferingPolicy(kBufferData);
  if (data->size())
    resource->SetResourceBuffer(data);
  resource->SetCacheIdentifier(cache_identifier);
  resource->Finish(base::TimeTicks(), task_runner_.get());

  AddToMemoryCacheIfNeeded(params, resource);
  return resource;
}

Resource* ResourceFetcher::ResourceForBlockedRequest(
    const FetchParameters& params,
    const ResourceFactory& factory,
    ResourceRequestBlockedReason blocked_reason,
    ResourceClient* client) {
  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  if (client)
    client->SetResource(resource, task_runner_.get());
  resource->FinishAsError(ResourceError::CancelledDueToAccessCheckError(
                              params.Url(), blocked_reason),
                          task_runner_.get());
  return resource;
}

void ResourceFetcher::MakePreloadedResourceBlockOnloadIfNeeded(
    Resource* resource,
    const FetchParameters& params) {
  // TODO(yoav): Test that non-blocking resources (video/audio/track) continue
  // to not-block even after being preloaded and discovered.
  if (resource && resource->Loader() &&
      resource->IsLoadEventBlockingResourceType() &&
      resource->IsLinkPreload() && !params.IsLinkPreload() &&
      non_blocking_loaders_.Contains(resource->Loader())) {
    non_blocking_loaders_.erase(resource->Loader());
    loaders_.insert(resource->Loader());
  }
}

void ResourceFetcher::UpdateMemoryCacheStats(Resource* resource,
                                             RevalidationPolicy policy,
                                             const FetchParameters& params,
                                             const ResourceFactory& factory,
                                             bool is_static_data) const {
  if (is_static_data)
    return;

  if (params.IsSpeculativePreload() || params.IsLinkPreload()) {
    DEFINE_RESOURCE_HISTOGRAM("Preload.");
  } else {
    DEFINE_RESOURCE_HISTOGRAM("");
  }

  // Aims to count Resource only referenced from MemoryCache (i.e. what would be
  // dead if MemoryCache holds weak references to Resource). Currently we check
  // references to Resource from ResourceClient and |m_preloads| only, because
  // they are major sources of references.
  if (resource && !resource->IsAlive() && !ContainsAsPreload(resource)) {
    DEFINE_RESOURCE_HISTOGRAM("Dead.");
  }

  // Async (and defer) scripts may have more cache misses, track them
  // separately. See https://crbug.com/1043679 for context.
  if (params.Defer() != FetchParameters::DeferOption::kNoDefer &&
      factory.GetType() == ResourceType::kScript) {
    UMA_HISTOGRAM_ENUMERATION(RESOURCE_HISTOGRAM_PREFIX "AsyncScript", policy);
  }
}

bool ResourceFetcher::ContainsAsPreload(Resource* resource) const {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  return it != preloads_.end() && it->value == resource;
}

void ResourceFetcher::RemovePreload(Resource* resource) {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  if (it == preloads_.end())
    return;
  if (it->value == resource)
    preloads_.erase(it);
}

base::Optional<ResourceRequestBlockedReason> ResourceFetcher::PrepareRequest(
    FetchParameters& params,
    const ResourceFactory& factory,
    WebScopedVirtualTimePauser& virtual_time_pauser) {
  ResourceRequest& resource_request = params.MutableResourceRequest();
  ResourceType resource_type = factory.GetType();
  const ResourceLoaderOptions& options = params.Options();

  DCHECK(options.synchronous_policy == kRequestAsynchronously ||
         resource_type == ResourceType::kRaw ||
         resource_type == ResourceType::kXSLStyleSheet);

  params.OverrideContentType(factory.ContentType());

  // No CSP reports are sent for:
  //
  // Speculative preload
  // ===================
  // This avoids sending 2 reports for a single resource (preload + real load).
  // Moreover the speculative preload are 'speculative', it might not even be
  // possible to issue a real request.
  //
  // Stale revalidations
  // ===================
  // Web browser should not send violation reports for stale revalidations. The
  // initial request was allowed. In theory, the revalidation request should be
  // allowed as well. However, some <meta> CSP header might have been added in
  // the meantime. See https://crbug.com/1070117.
  //
  // Note: Ideally, stale revalidations should bypass every checks. In practise,
  // they are run and block the request. Bypassing all security checks could be
  // risky and probably doesn't really worth it. They are very rarely blocked.
  ReportingDisposition reporting_disposition =
      params.IsSpeculativePreload() || params.IsStaleRevalidation()
          ? ReportingDisposition::kSuppressReporting
          : ReportingDisposition::kReport;

  // Note that resource_request.GetRedirectInfo() may be non-null here since
  // e.g. ThreadableLoader may create a new Resource from a ResourceRequest that
  // originates from the ResourceRequest passed to the redirect handling
  // callback.

  // Before modifying the request for CSP, evaluate report-only headers. This
  // allows site owners to learn about requests that are being modified
  // (e.g. mixed content that is being upgraded by upgrade-insecure-requests).
  const base::Optional<ResourceRequest::RedirectInfo>& redirect_info =
      resource_request.GetRedirectInfo();
  const KURL& url_before_redirects =
      redirect_info ? redirect_info->original_url : params.Url();
  const ResourceRequestHead::RedirectStatus redirect_status =
      redirect_info ? ResourceRequestHead::RedirectStatus::kFollowedRedirect
                    : ResourceRequestHead::RedirectStatus::kNoRedirect;
  Context().CheckCSPForRequest(
      resource_request.GetRequestContext(),
      resource_request.GetRequestDestination(),
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()), options,
      reporting_disposition,
      MemoryCache::RemoveFragmentIdentifierIfNeeded(url_before_redirects),
      redirect_status);

  // This may modify params.Url() (via the resource_request argument).
  Context().PopulateResourceRequest(
      resource_type, params.GetClientHintsPreferences(),
      params.GetResourceWidth(), resource_request, options);

  if (!params.Url().IsValid())
    return ResourceRequestBlockedReason::kOther;

  ResourceLoadPriority computed_load_priority = resource_request.Priority();
  // We should only compute the priority for ResourceRequests whose priority has
  // not already been set.
  if (!resource_request.PriorityHasBeenSet()) {
    computed_load_priority = ComputeLoadPriority(
        resource_type, params.GetResourceRequest(),
        ResourcePriority::kNotVisible, params.Defer(),
        params.GetSpeculativePreloadType(), params.IsLinkPreload());
  }

  DCHECK_NE(computed_load_priority, ResourceLoadPriority::kUnresolved);
  resource_request.SetPriority(computed_load_priority);

  if (resource_request.GetCacheMode() == mojom::FetchCacheMode::kDefault) {
    resource_request.SetCacheMode(Context().ResourceRequestCachePolicy(
        resource_request, resource_type, params.Defer()));
  }
  if (resource_request.GetRequestContext() ==
      mojom::RequestContextType::UNSPECIFIED) {
    resource_request.SetRequestContext(
        DetermineRequestContext(resource_type, kImageNotImageSet));
    resource_request.SetRequestDestination(
        DetermineRequestDestination(resource_type));
  }
  if (resource_type == ResourceType::kLinkPrefetch)
    resource_request.SetPurposeHeader("prefetch");

  // Indicate whether the network stack can return a stale resource. If a
  // stale resource is returned a StaleRevalidation request will be scheduled.
  // Explicitly disallow stale responses for fetchers that don't have SWR
  // enabled (via origin trial), and non-GET requests.
  resource_request.SetAllowStaleResponse(resource_request.HttpMethod() ==
                                             http_names::kGET &&
                                         !params.IsStaleRevalidation());

  SetReferrer(resource_request, properties_->GetFetchClientSettingsObject(),
              GetUseCounter());

  resource_request.SetExternalRequestStateFromRequestorAddressSpace(
      properties_->GetFetchClientSettingsObject().GetAddressSpace());

  Context().AddAdditionalRequestHeaders(resource_request);

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourcePrioritySet",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_request.InspectorId())),
      "data", ResourcePrioritySetData(resource_request.Priority()));

  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      Context().CanRequest(resource_type, resource_request, url, options,
                           reporting_disposition,
                           resource_request.GetRedirectInfo());

  if (Context().CalculateIfAdSubresource(resource_request, resource_type,
                                         options.initiator_info))
    resource_request.SetIsAdResource();

  if (blocked_reason)
    return blocked_reason;

  // For initial requests, call PrepareRequest() here before revalidation
  // policy is determined.
  Context().PrepareRequest(resource_request, options.initiator_info,
                           virtual_time_pauser, resource_type);

  if (!params.Url().IsValid())
    return ResourceRequestBlockedReason::kOther;

  if (resource_request.GetCredentialsMode() ==
      network::mojom::CredentialsMode::kOmit) {
    // See comments at network::ResourceRequest::credentials_mode.
    resource_request.SetAllowStoredCredentials(false);
  }

  return base::nullopt;
}

Resource* ResourceFetcher::RequestResource(FetchParameters& params,
                                           const ResourceFactory& factory,
                                           ResourceClient* client) {
  base::AutoReset<bool> r(&is_in_request_resource_, true);

  if (should_log_request_as_invalid_in_imported_document_) {
    DCHECK(properties_->IsDetached());
    base::debug::DumpWithoutCrashing();
  }

  // If detached, we do very early return here to skip all processing below.
  if (properties_->IsDetached()) {
    return ResourceForBlockedRequest(
        params, factory, ResourceRequestBlockedReason::kOther, client);
  }

  if (resource_load_observer_) {
    resource_load_observer_->DidStartRequest(params, factory.GetType());
  }

  // Otherwise, we assume we can send network requests and the fetch client's
  // settings object's origin is non-null.
  DCHECK(properties_->GetFetchClientSettingsObject().GetSecurityOrigin());

  uint64_t identifier = CreateUniqueIdentifier();
  ResourceRequest& resource_request = params.MutableResourceRequest();
  resource_request.SetInspectorId(identifier);
  resource_request.SetFromOriginDirtyStyleSheet(
      params.IsFromOriginDirtyStyleSheet());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID", TRACE_ID_LOCAL(identifier)),
      "beginData", BeginResourceLoadData(resource_request));
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_THREAD_SAFE(
      "Blink.Fetch.RequestResourceTime");
  TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url",
               params.Url().GetString().Utf8());

  // |resource_request|'s origin can be null here, corresponding to the "client"
  // value in the spec. In that case client's origin is used.
  if (!resource_request.RequestorOrigin()) {
    resource_request.SetRequestorOrigin(
        properties_->GetFetchClientSettingsObject().GetSecurityOrigin());
  }

  const ResourceType resource_type = factory.GetType();

  WebScopedVirtualTimePauser pauser;

  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      PrepareRequest(params, factory, pauser);
  if (blocked_reason) {
    return ResourceForBlockedRequest(params, factory, blocked_reason.value(),
                                     client);
  }

  Resource* resource = nullptr;
  RevalidationPolicy policy = RevalidationPolicy::kLoad;

  bool is_data_url = resource_request.Url().ProtocolIsData();
  bool is_static_data = is_data_url || archive_;
  bool is_stale_revalidation = params.IsStaleRevalidation();
  if (!is_stale_revalidation && is_static_data) {
    resource = ResourceForStaticData(params, factory);
    if (resource) {
      policy =
          DetermineRevalidationPolicy(resource_type, params, *resource, true);
    } else if (!is_data_url && archive_) {
      // Abort the request if the archive doesn't contain the resource, except
      // in the case of data URLs which might have resources such as fonts that
      // need to be decoded only on demand. These data URLs are allowed to be
      // processed using the normal ResourceFetcher machinery.
      return ResourceForBlockedRequest(
          params, factory, ResourceRequestBlockedReason::kOther, client);
    }
  }

  if (!is_stale_revalidation && !resource) {
    resource = MatchPreload(params, resource_type);
    if (resource) {
      policy = RevalidationPolicy::kUse;
      // If |params| is for a blocking resource and a preloaded resource is
      // found, we may need to make it block the onload event.
      MakePreloadedResourceBlockOnloadIfNeeded(resource, params);
    } else if (IsMainThread()) {
      resource = GetMemoryCache()->ResourceForURL(
          params.Url(), GetCacheIdentifier(params.Url()));
      if (resource) {
        policy = DetermineRevalidationPolicy(resource_type, params, *resource,
                                             is_static_data);
      }
    }
  }

  UpdateMemoryCacheStats(resource, policy, params, factory, is_static_data);

  switch (policy) {
    case RevalidationPolicy::kReload:
      GetMemoryCache()->Remove(resource);
      FALLTHROUGH;
    case RevalidationPolicy::kLoad:
      resource = CreateResourceForLoading(params, factory);
      break;
    case RevalidationPolicy::kRevalidate:
      InitializeRevalidation(resource_request, resource);
      break;
    case RevalidationPolicy::kUse:
      if (resource_request.AllowsStaleResponse() &&
          resource->ShouldRevalidateStaleResponse()) {
        ScheduleStaleRevalidate(resource);
      }

      break;
  }
  DCHECK(resource);
  DCHECK_EQ(resource->GetType(), resource_type);

  if (policy != RevalidationPolicy::kUse)
    resource->VirtualTimePauser() = std::move(pauser);

  if (client)
    client->SetResource(resource, task_runner_.get());

  // TODO(yoav): It is not clear why preloads are exempt from this check. Can we
  // remove the exemption?
  if (!params.IsSpeculativePreload() || policy != RevalidationPolicy::kUse) {
    // When issuing another request for a resource that is already in-flight
    // make sure to not demote the priority of the in-flight request. If the new
    // request isn't at the same priority as the in-flight request, only allow
    // promotions. This can happen when a visible image's priority is increased
    // and then another reference to the image is parsed (which would be at a
    // lower priority).
    if (resource_request.Priority() > resource->GetResourceRequest().Priority())
      resource->DidChangePriority(resource_request.Priority(), 0);
    // TODO(yoav): I'd expect the stated scenario to not go here, as its policy
    // would be Use.
  }

  // If only the fragment identifiers differ, it is the same resource.
  DCHECK(EqualIgnoringFragmentIdentifier(resource->Url(), params.Url()));
  if (policy == RevalidationPolicy::kUse &&
      resource->GetStatus() == ResourceStatus::kCached &&
      !cached_resources_map_.Contains(
          MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()))) {
    // Loaded from MemoryCache.
    DidLoadResourceFromMemoryCache(resource, resource_request, is_static_data);
  }
  if (!is_stale_revalidation) {
    String resource_url =
        MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
    cached_resources_map_.Set(resource_url, resource);
    if (PriorityObserverMapCreated() &&
        PriorityObservers()->Contains(resource_url)) {
      // Resolve the promise.
      std::move(PriorityObservers()->Take(resource_url))
          .Run(static_cast<int>(resource->GetResourceRequest().Priority()));
    }
  }

  // Image loaders are by default added to |loaders_|, and are therefore
  // load-blocking. Lazy loaded images that are eventually fetched, however,
  // should always be added to |non_blocking_loaders_|, as they are never
  // load-blocking.
  LoadBlockingPolicy load_blocking_policy = LoadBlockingPolicy::kDefault;
  if (resource->GetType() == ResourceType::kImage) {
    image_resources_.insert(resource);
#if DCHECK_IS_ON()
    DCHECK(!not_loaded_image_resources_is_being_iterated_);
#endif
    not_loaded_image_resources_.insert(resource);
    if (params.GetImageRequestBehavior() ==
        FetchParameters::kNonBlockingImage) {
      load_blocking_policy = LoadBlockingPolicy::kForceNonBlockingLoad;
    }
  }

  // Returns with an existing resource if the resource does not need to start
  // loading immediately. If revalidation policy was determined as |Revalidate|,
  // the resource was already initialized for the revalidation here, but won't
  // start loading.
  if (ResourceNeedsLoad(resource, params, policy)) {
    if (!StartLoad(resource,
                   std::move(params.MutableResourceRequest().MutableBody()),
                   load_blocking_policy)) {
      resource->FinishAsError(ResourceError::CancelledError(params.Url()),
                              task_runner_.get());
    }
  }

  if (policy != RevalidationPolicy::kUse)
    InsertAsPreloadIfNecessary(resource, params, resource_type);

  if (resource->InspectorId() != identifier ||
      (!resource->StillNeedsLoad() && !resource->IsLoading())) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
        TRACE_ID_WITH_SCOPE("BlinkResourceID", TRACE_ID_LOCAL(identifier)),
        "endData", EndResourceLoadFailData());
  }

  return resource;
}

void ResourceFetcher::MarkFirstPaint() {
  scheduler_->MarkFirstPaint();
}

void ResourceFetcher::MarkFirstContentfulPaint() {
  scheduler_->MarkFirstContentfulPaint();
}

void ResourceFetcher::ResourceTimingReportTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &resource_timing_report_timer_);
  Vector<scoped_refptr<ResourceTimingInfo>> timing_reports;
  timing_reports.swap(scheduled_resource_timing_reports_);
  for (const auto& timing_info : timing_reports)
    Context().AddResourceTiming(*timing_info);
}

void ResourceFetcher::InitializeRevalidation(
    ResourceRequest& revalidating_request,
    Resource* resource) {
  DCHECK(resource);
  DCHECK(GetMemoryCache()->Contains(resource));
  DCHECK(resource->IsLoaded());
  DCHECK(resource->CanUseCacheValidator());
  DCHECK(!resource->IsCacheValidator());
  DCHECK_EQ(properties_->GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // RawResource doesn't support revalidation.
  CHECK(!IsRawResource(*resource));

  revalidating_request.SetIsRevalidating(true);

  const AtomicString& last_modified =
      resource->GetResponse().HttpHeaderField(http_names::kLastModified);
  const AtomicString& e_tag =
      resource->GetResponse().HttpHeaderField(http_names::kETag);
  if (!last_modified.IsEmpty() || !e_tag.IsEmpty()) {
    DCHECK_NE(mojom::FetchCacheMode::kBypassCache,
              revalidating_request.GetCacheMode());
    if (revalidating_request.GetCacheMode() ==
        mojom::FetchCacheMode::kValidateCache) {
      revalidating_request.SetHttpHeaderField(http_names::kCacheControl,
                                              "max-age=0");
    }
  }
  if (!last_modified.IsEmpty()) {
    revalidating_request.SetHttpHeaderField(http_names::kIfModifiedSince,
                                            last_modified);
  }
  if (!e_tag.IsEmpty())
    revalidating_request.SetHttpHeaderField(http_names::kIfNoneMatch, e_tag);

  resource->SetRevalidatingRequest(revalidating_request);
}

std::unique_ptr<WebURLLoader> ResourceFetcher::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options) {
  DCHECK(!GetProperties().IsDetached());
  DCHECK(loader_factory_);
  for (auto& bundle : subresource_web_bundles_) {
    if (!bundle->CanHandleRequest(request.Url()))
      continue;
    ResourceLoaderOptions new_options(options);
    new_options.url_loader_factory = base::MakeRefCounted<base::RefCountedData<
        mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>>(
        bundle->GetURLLoaderFactory());
    return loader_factory_->CreateURLLoader(request, new_options, task_runner_);
  }
  return loader_factory_->CreateURLLoader(request, options, task_runner_);
}

std::unique_ptr<WebCodeCacheLoader> ResourceFetcher::CreateCodeCacheLoader() {
  DCHECK(!GetProperties().IsDetached());
  DCHECK(loader_factory_);
  return loader_factory_->CreateCodeCacheLoader();
}

void ResourceFetcher::AddToMemoryCacheIfNeeded(const FetchParameters& params,
                                               Resource* resource) {
  if (!ShouldResourceBeAddedToMemoryCache(params, resource))
    return;

  GetMemoryCache()->Add(resource);
}

Resource* ResourceFetcher::CreateResourceForLoading(
    const FetchParameters& params,
    const ResourceFactory& factory) {
  const String cache_identifier =
      GetCacheIdentifier(params.GetResourceRequest().Url());
  DCHECK(!IsMainThread() || params.IsStaleRevalidation() ||
         !GetMemoryCache()->ResourceForURL(params.GetResourceRequest().Url(),
                                           cache_identifier));

  RESOURCE_LOADING_DVLOG(1) << "Loading Resource for "
                            << params.GetResourceRequest().Url().ElidedString();

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetLinkPreload(params.IsLinkPreload());
  resource->SetCacheIdentifier(cache_identifier);

  AddToMemoryCacheIfNeeded(params, resource);
  return resource;
}

void ResourceFetcher::StorePerformanceTimingInitiatorInformation(
    Resource* resource) {
  const AtomicString& fetch_initiator = resource->Options().initiator_info.name;
  if (fetch_initiator == fetch_initiator_type_names::kInternal)
    return;

  scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
      fetch_initiator, base::TimeTicks::Now(),
      resource->GetResourceRequest().GetRequestContext(),
      resource->GetResourceRequest().GetRequestDestination());

  resource_timing_info_map_.insert(resource, std::move(info));
}

void ResourceFetcher::RecordResourceTimingOnRedirect(
    Resource* resource,
    const ResourceResponse& redirect_response,
    const KURL& new_url) {
  ResourceTimingInfoMap::iterator it = resource_timing_info_map_.find(resource);
  if (it != resource_timing_info_map_.end())
    it->value->AddRedirect(redirect_response, new_url);
}

static bool IsDownloadOrStreamRequest(const ResourceRequest& request) {
  // Never use cache entries for DownloadToBlob / UseStreamOnResponse requests.
  // The data will be delivered through other paths.
  return request.DownloadToBlob() || request.UseStreamOnResponse();
}

Resource* ResourceFetcher::MatchPreload(const FetchParameters& params,
                                        ResourceType type) {
  // TODO(crbug.com/1099975): PreloadKey should be modified to also take into
  // account the DOMWrapperWorld corresponding to the resource. This is because
  // we probably don't want to share preloaded resources across different
  // DOMWrapperWorlds to ensure predicatable behavior for preloads.
  auto it = preloads_.find(PreloadKey(params.Url(), type));
  if (it == preloads_.end())
    return nullptr;

  Resource* resource = it->value;

  if (resource->MustRefetchDueToIntegrityMetadata(params)) {
    if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
      PrintPreloadWarning(resource, Resource::MatchStatus::kIntegrityMismatch);
    return nullptr;
  }

  if (params.IsSpeculativePreload())
    return resource;
  if (params.IsLinkPreload()) {
    resource->SetLinkPreload(true);
    return resource;
  }

  const ResourceRequest& request = params.GetResourceRequest();
  if (request.DownloadToBlob()) {
    PrintPreloadWarning(resource, Resource::MatchStatus::kBlobRequest);
    return nullptr;
  }

  if (IsImageResourceDisallowedToBeReused(*resource)) {
    PrintPreloadWarning(resource, Resource::MatchStatus::kImageLoadingDisabled);
    return nullptr;
  }

  const Resource::MatchStatus match_status = resource->CanReuse(params);
  if (match_status != Resource::MatchStatus::kOk) {
    PrintPreloadWarning(resource, match_status);
    return nullptr;
  }

  resource->MatchPreload(params);
  preloads_.erase(it);
  matched_preloads_.push_back(resource);
  return resource;
}

void ResourceFetcher::PrintPreloadWarning(Resource* resource,
                                          Resource::MatchStatus status) {
  if (!resource->IsLinkPreload())
    return;

  StringBuilder builder;
  builder.Append("A preload for '");
  builder.Append(resource->Url());
  builder.Append("' is found, but is not used ");

  switch (status) {
    case Resource::MatchStatus::kOk:
      NOTREACHED();
      break;
    case Resource::MatchStatus::kUnknownFailure:
      builder.Append("due to an unknown reason.");
      break;
    case Resource::MatchStatus::kIntegrityMismatch:
      builder.Append("due to an integrity mismatch.");
      break;
    case Resource::MatchStatus::kBlobRequest:
      builder.Append("because the new request loads the content as a blob.");
      break;
    case Resource::MatchStatus::kImageLoadingDisabled:
      builder.Append("because image loading is disabled.");
      break;
    case Resource::MatchStatus::kSynchronousFlagDoesNotMatch:
      builder.Append("because the new request is synchronous.");
      break;
    case Resource::MatchStatus::kRequestModeDoesNotMatch:
      builder.Append("because the request mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kRequestCredentialsModeDoesNotMatch:
      builder.Append("because the request credentials mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kKeepaliveSet:
      builder.Append("because the keepalive flag is set.");
      break;
    case Resource::MatchStatus::kRequestMethodDoesNotMatch:
      builder.Append("because the request HTTP method does not match.");
      break;
    case Resource::MatchStatus::kRequestHeadersDoNotMatch:
      builder.Append("because the request headers do not match.");
      break;
  }
  console_logger_->AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                                     mojom::ConsoleMessageLevel::kWarning,
                                     builder.ToString());
}

void ResourceFetcher::InsertAsPreloadIfNecessary(Resource* resource,
                                                 const FetchParameters& params,
                                                 ResourceType type) {
  if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
    return;
  DCHECK(!params.IsStaleRevalidation());
  // CSP web tests verify that preloads are subject to access checks by
  // seeing if they are in the `preload started` list. Therefore do not add
  // them to the list if the load is immediately denied.
  if (resource->LoadFailedOrCanceled() &&
      resource->GetResourceError().IsAccessCheck()) {
    return;
  }
  PreloadKey key(params.Url(), type);
  if (preloads_.find(key) != preloads_.end())
    return;

  preloads_.insert(key, resource);
  resource->MarkAsPreload();
  if (preloaded_urls_for_test_)
    preloaded_urls_for_test_->insert(resource->Url().GetString());
}

bool ResourceFetcher::IsImageResourceDisallowedToBeReused(
    const Resource& existing_resource) const {
  // When images are disabled, don't ever load images, even if the image is
  // cached or it is a data: url. In this case:
  // - remove the image from the memory cache, and
  // - create a new resource but defer loading (this is done by
  //   ResourceNeedsLoad()).
  //
  // This condition must be placed before the condition on |is_static_data| to
  // prevent loading a data: URL.
  //
  // TODO(japhet): Can we get rid of one of these settings?

  if (existing_resource.GetType() != ResourceType::kImage)
    return false;

  return !Context().AllowImage(images_enabled_, existing_resource.Url());
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::DetermineRevalidationPolicy(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  RevalidationPolicy policy;
  const char* reason;
  std::tie(policy, reason) = DetermineRevalidationPolicyInternal(
      type, fetch_params, existing_resource, is_static_data);
  DCHECK(reason);

  RESOURCE_LOADING_DVLOG(1)
      << "ResourceFetcher::DetermineRevalidationPolicy "
      << "url = " << fetch_params.Url() << ", policy = " << GetNameFor(policy)
      << ", reason = \"" << reason << "\"";

  TRACE_EVENT_INSTANT2("blink", "ResourceFetcher::DetermineRevalidationPolicy",
                       TRACE_EVENT_SCOPE_THREAD, "policy", GetNameFor(policy),
                       "reason", reason);
  return policy;
}

const char* ResourceFetcher::GetNameFor(RevalidationPolicy policy) {
  switch (policy) {
    case RevalidationPolicy::kUse:
      return "use";
    case RevalidationPolicy::kRevalidate:
      return "revalidate";
    case RevalidationPolicy::kReload:
      return "reload";
    case RevalidationPolicy::kLoad:
      return "load";
  }
  NOTREACHED();
}

std::pair<ResourceFetcher::RevalidationPolicy, const char*>
ResourceFetcher::DetermineRevalidationPolicyInternal(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  const ResourceRequest& request = fetch_params.GetResourceRequest();

  if (IsDownloadOrStreamRequest(request)) {
    return {RevalidationPolicy::kReload,
            "It is for download or for streaming."};
  }

  if (IsImageResourceDisallowedToBeReused(existing_resource)) {
    return {RevalidationPolicy::kReload,
            "Reload due to 'allow image' settings."};
  }

  // If the existing resource is loading and the associated fetcher is not equal
  // to |this|, we must not use the resource. Otherwise, CSP violation may
  // happen in redirect handling.
  if (existing_resource.Loader() &&
      existing_resource.Loader()->Fetcher() != this) {
    return {RevalidationPolicy::kReload,
            "The existing resource is loading in a foreign fetcher."};
  }

  // It's hard to share a not-yet-referenced preloads via MemoryCache correctly.
  // A not-yet-matched preloads made by a foreign ResourceFetcher and stored in
  // the memory cache could be used without this block.
  if ((fetch_params.IsLinkPreload() || fetch_params.IsSpeculativePreload()) &&
      existing_resource.IsUnusedPreload()) {
    return {RevalidationPolicy::kReload,
            "The existing resource is an unused preload made "
            "from a foreign fetcher."};
  }

  // Checks if the resource has an explicit policy about integrity metadata.
  //
  // This is necessary because ScriptResource and CSSStyleSheetResource objects
  // do not keep the raw data around after the source is accessed once, so if
  // the resource is accessed from the MemoryCache for a second time, there is
  // no way to redo an integrity check.
  //
  // Thus, Blink implements a scheme where it caches the integrity information
  // for those resources after the first time it is checked, and if there is
  // another request for that resource, with the same integrity metadata, Blink
  // skips the integrity calculation. However, if the integrity metadata is a
  // mismatch, the MemoryCache must be skipped here, and a new request for the
  // resource must be made to get the raw data. This is expected to be an
  // uncommon case, however, as it implies two same-origin requests to the same
  // resource, but with different integrity metadata.
  if (existing_resource.MustRefetchDueToIntegrityMetadata(fetch_params)) {
    return {RevalidationPolicy::kReload, "Reload due to resource integrity."};
  }

  // If the same URL has been loaded as a different type, we need to reload.
  if (existing_resource.GetType() != type) {
    // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
    // We really should discard the new prefetch since the preload has more
    // specific type information! crbug.com/379893
    // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
    return {RevalidationPolicy::kReload, "Reload due to type mismatch."};
  }

  // If resource was populated from archive or data: url, use it.
  // This doesn't necessarily mean that |resource| was just created by using
  // ResourceForStaticData().
  if (is_static_data) {
    return {RevalidationPolicy::kUse, "Use the existing static resource."};
  }

  if (existing_resource.CanReuse(fetch_params) != Resource::MatchStatus::kOk) {
    return {RevalidationPolicy::kReload, "Reload due to Resource::CanReuse."};
  }

  // Don't reload resources while pasting.
  if (allow_stale_resources_) {
    return {RevalidationPolicy::kUse,
            "Use the existing resource due to |allow_stale_resources_|."};
  }

  // FORCE_CACHE uses the cache no matter what.
  if (request.GetCacheMode() == mojom::FetchCacheMode::kForceCache) {
    return {RevalidationPolicy::kUse,
            "Use the existing resource due to cache-mode: 'force-cache'."};
  }

  // Don't reuse resources with Cache-control: no-store.
  if (existing_resource.HasCacheControlNoStoreHeader()) {
    return {RevalidationPolicy::kReload,
            "Reload due to cache-control: no-store."};
  }

  // During the initial load, avoid loading the same resource multiple times for
  // a single document, even if the cache policies would tell us to. We also
  // group loads of the same resource together. Raw resources are exempted, as
  // XHRs fall into this category and may have user-set Cache-Control: headers
  // or other factors that require separate requests.
  if (type != ResourceType::kRaw) {
    if (!properties_->IsLoadComplete() &&
        cached_resources_map_.Contains(
            MemoryCache::RemoveFragmentIdentifierIfNeeded(
                existing_resource.Url()))) {
      return {RevalidationPolicy::kUse,
              "Avoid making multiple requests for the same URL "
              "during the initial load."};
    }
    if (existing_resource.IsLoading()) {
      return {RevalidationPolicy::kUse,
              "Use the existing resource because it's being loaded."};
    }
  }

  // RELOAD always reloads
  if (request.GetCacheMode() == mojom::FetchCacheMode::kBypassCache) {
    return {RevalidationPolicy::kReload, "Reload due to cache-mode: 'reload'."};
  }

  // We'll try to reload the resource if it failed last time.
  if (existing_resource.ErrorOccurred()) {
    return {RevalidationPolicy::kReload,
            "Reload because the existing resource has failed loading."};
  }

  // List of available images logic allows images to be re-used without cache
  // validation. We restrict this only to images from memory cache which are the
  // same as the version in the current document.
  if (type == ResourceType::kImage &&
      &existing_resource == CachedResource(request.Url())) {
    return {RevalidationPolicy::kUse,
            "Images can be reused without cache validation."};
  }

  if (existing_resource.MustReloadDueToVaryHeader(request)) {
    return {RevalidationPolicy::kReload, "Reload due to vary header."};
  }

  // If any of the redirects in the chain to loading the resource were not
  // cacheable, we cannot reuse our cached resource.
  if (!existing_resource.CanReuseRedirectChain()) {
    return {RevalidationPolicy::kReload,
            "Reload due to an uncacheable redirect."};
  }

  // Check if the cache headers requires us to revalidate (cache expiration for
  // example).
  if (request.GetCacheMode() == mojom::FetchCacheMode::kValidateCache ||
      existing_resource.MustRevalidateDueToCacheHeaders(
          request.AllowsStaleResponse()) ||
      request.CacheControlContainsNoCache()) {
    // Revalidation is harmful for non-matched preloads because it may lead to
    // sharing one preloaded resource among multiple ResourceFetchers.
    if (existing_resource.IsUnusedPreload()) {
      return {RevalidationPolicy::kReload,
              "Revalidation is harmful for non-matched preloads."};
    }

    // See if the resource has usable ETag or Last-modified headers. If the page
    // is controlled by the ServiceWorker, we choose the Reload policy because
    // the revalidation headers should not be exposed to the
    // ServiceWorker.(crbug.com/429570)
    //
    // TODO(falken): If the controller has no fetch event handler, we probably
    // can treat it as not being controlled in the S13nSW case. In the
    // non-S13nSW, we don't know what controller the request will ultimately go
    // to (due to skipWaiting) so be conservative.
    if (existing_resource.CanUseCacheValidator() &&
        properties_->GetControllerServiceWorkerMode() ==
            mojom::ControllerServiceWorkerMode::kNoController) {
      // If the resource is already a cache validator but not started yet, the
      // |Use| policy should be applied to subsequent requests.
      if (existing_resource.IsCacheValidator()) {
        DCHECK(existing_resource.StillNeedsLoad());
        return {RevalidationPolicy::kUse,
                "Merged to the revalidate request which has not yet started."};
      }
      return {RevalidationPolicy::kRevalidate, ""};
    }

    // No, must reload.
    return {RevalidationPolicy::kReload,
            "Reload due to missing cache validators."};
  }

  return {RevalidationPolicy::kUse,
          "Use the existing resource because there is no reason not to do so."};
}

void ResourceFetcher::SetAutoLoadImages(bool enable) {
  if (enable == auto_load_images_)
    return;

  auto_load_images_ = enable;

  if (!auto_load_images_)
    return;

  ReloadImagesIfNotDeferred();
}

void ResourceFetcher::SetImagesEnabled(bool enable) {
  if (enable == images_enabled_)
    return;

  images_enabled_ = enable;

  if (!images_enabled_)
    return;

  ReloadImagesIfNotDeferred();
}

bool ResourceFetcher::ShouldDeferImageLoad(const KURL& url) const {
  return !Context().AllowImage(images_enabled_, url) || !auto_load_images_;
}

void ResourceFetcher::ReloadImagesIfNotDeferred() {
#if DCHECK_IS_ON()
  DCHECK(!not_loaded_image_resources_is_being_iterated_);
  base::AutoReset<bool> is_being_modified_resetter(
      &not_loaded_image_resources_is_being_iterated_, true);
#endif
  for (Resource* resource : not_loaded_image_resources_) {
    DCHECK_EQ(resource->GetType(), ResourceType::kImage);
    if (resource->StillNeedsLoad() && !ShouldDeferImageLoad(resource->Url()))
      StartLoad(resource);
  }
}

FetchContext& ResourceFetcher::Context() const {
  return *context_;
}

void ResourceFetcher::ClearContext() {
  scheduler_->Shutdown();
  ClearPreloads(ResourceFetcher::kClearAllPreloads);

  {
    // This block used to be
    //  context_ = Context().Detach();
    // While we are splitting FetchContext to multiple classes we need to call
    // "detach" for multiple objects in a coordinated manner. See
    // https://crbug.com/914739 for the progress.
    // TODO(yhirano): Remove the cross-class dependency.
    context_ = Context().Detach();
    properties_->Detach();
  }

  resource_load_observer_ = nullptr;
  use_counter_->Detach();
  console_logger_->Detach();
  loader_factory_ = nullptr;

  unused_preloads_timer_.Cancel();

  // Make sure the only requests still going are keepalive requests.
  // Callers of ClearContext() should be calling StopFetching() prior
  // to this, but it's possible for additional requests to start during
  // StopFetching() (e.g., fallback fonts that only trigger when the
  // first choice font failed to load).
  StopFetching();

  if (!loaders_.IsEmpty() || !non_blocking_loaders_.IsEmpty()) {
    // There are some keepalive requests.
    // The use of WrapPersistent creates a reference cycle intentionally,
    // to keep the ResourceFetcher and ResourceLoaders alive until the requests
    // complete or the timer fires.
    keepalive_loaders_task_handle_ = PostDelayedCancellableTask(
        *task_runner_, FROM_HERE,
        WTF::Bind(&ResourceFetcher::StopFetchingIncludingKeepaliveLoaders,
                  WrapPersistent(this)),
        kKeepaliveLoadersTimeout);
  }
}

int ResourceFetcher::BlockingRequestCount() const {
  return loaders_.size();
}

int ResourceFetcher::NonblockingRequestCount() const {
  return non_blocking_loaders_.size();
}

int ResourceFetcher::ActiveRequestCount() const {
  return loaders_.size() + non_blocking_loaders_.size();
}

void ResourceFetcher::EnableIsPreloadedForTest() {
  if (preloaded_urls_for_test_)
    return;
  preloaded_urls_for_test_ = std::make_unique<HashSet<String>>();

  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    preloaded_urls_for_test_->insert(resource->Url().GetString());
  }
}

bool ResourceFetcher::IsPreloadedForTest(const KURL& url) const {
  DCHECK(preloaded_urls_for_test_);
  return preloaded_urls_for_test_->Contains(url.GetString());
}

void ResourceFetcher::ClearPreloads(ClearPreloadsPolicy policy) {
  Vector<PreloadKey> keys_to_be_removed;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (policy == kClearAllPreloads || !resource->IsLinkPreload()) {
      GetMemoryCache()->Remove(resource);
      keys_to_be_removed.push_back(pair.key);
    }
  }
  preloads_.RemoveAll(keys_to_be_removed);

  matched_preloads_.clear();
}

void ResourceFetcher::ScheduleWarnUnusedPreloads() {
  // If preloads_ is not empty here, it's full of link
  // preloads, as speculative preloads should have already been cleared when
  // parsing finished.
  if (preloads_.IsEmpty())
    return;
  unused_preloads_timer_ = PostDelayedCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::Bind(&ResourceFetcher::WarnUnusedPreloads, WrapWeakPersistent(this)),
      kUnusedPreloadTimeout);
}

void ResourceFetcher::WarnUnusedPreloads() {
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (!resource || !resource->IsLinkPreload() || !resource->IsUnusedPreload())
      continue;
    String message =
        "The resource " + resource->Url().GetString() + " was preloaded " +
        "using link preload but not used within a few seconds from the " +
        "window's load event. Please make sure it has an appropriate `as` " +
        "value and it is preloaded intentionally.";
    console_logger_->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning, message);
  }
}

void ResourceFetcher::HandleLoaderFinish(Resource* resource,
                                         base::TimeTicks response_end,
                                         LoaderFinishType type,
                                         uint32_t inflight_keepalive_bytes,
                                         bool should_report_corb_blocking) {
  DCHECK(resource);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  ResourceLoader* loader = resource->Loader();
  if (type == kDidFinishFirstPartInMultipart) {
    // When loading a multipart resource, make the loader non-block when
    // finishing loading the first part.
    MoveResourceLoaderToNonBlocking(loader);
  } else {
    RemoveResourceLoader(loader);
    DCHECK(!non_blocking_loaders_.Contains(loader));
  }
  DCHECK(!loaders_.Contains(loader));

  const int64_t encoded_data_length =
      resource->GetResponse().EncodedDataLength();

  if (scoped_refptr<ResourceTimingInfo> info =
          resource_timing_info_map_.Take(resource)) {
    if (resource->GetResponse().IsHTTP()) {
      PopulateAndAddResourceTimingInfo(resource, info, response_end,
                                       encoded_data_length);
      auto receiver = Context().TakePendingWorkerTimingReceiver(
          resource->GetResponse().RequestId());
      info->SetWorkerTimingReceiver(std::move(receiver));
      if (resource->Options().request_initiator_context == kDocumentContext)
        Context().AddResourceTiming(*info);
    }
  }

  resource->VirtualTimePauser().UnpauseVirtualTime();
  if (type == kDidFinishLoading) {
    resource->Finish(response_end, task_runner_.get());

    // Since this resource came from the network stack we only schedule a stale
    // while revalidate request if the network asked us to. If we called
    // ShouldRevalidateStaleResponse here then the resource would be checking
    // the freshness based on current time. It is possible that the resource
    // is fresh at the time of the network stack handling but not at the time
    // handling here and we should not be forcing a revalidation in that case.
    // eg. network stack returning a resource with max-age=0.
    if (resource->GetResourceRequest().AllowsStaleResponse() &&
        resource->StaleRevalidationRequested()) {
      ScheduleStaleRevalidate(resource);
    }
  }
  if (resource_load_observer_) {
    DCHECK(!IsDetached());
    resource_load_observer_->DidFinishLoading(
        resource->InspectorId(), response_end, encoded_data_length,
        resource->GetResponse().DecodedBodyLength(),
        should_report_corb_blocking);
  }
}

void ResourceFetcher::HandleLoaderError(Resource* resource,
                                        const ResourceError& error,
                                        uint32_t inflight_keepalive_bytes) {
  DCHECK(resource);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  RemoveResourceLoader(resource->Loader());

  if (scoped_refptr<ResourceTimingInfo> info =
          resource_timing_info_map_.Take(resource)) {
    PopulateAndAddResourceTimingInfo(
        resource, info, info->InitialTime(),
        resource->GetResponse().EncodedDataLength());
    if (resource->Options().request_initiator_context == kDocumentContext)
      Context().AddResourceTiming(*info);
  }

  resource->VirtualTimePauser().UnpauseVirtualTime();
  if (error.IsCancellation())
    RemovePreload(resource);
  if (network_utils::IsCertificateTransparencyRequiredError(
          error.ErrorCode())) {
    use_counter_->CountUse(
        mojom::WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad);
  }
  resource->FinishAsError(error, task_runner_.get());
  if (resource_load_observer_) {
    DCHECK(!IsDetached());
    resource_load_observer_->DidFailLoading(
        resource->LastResourceRequest().Url(), resource->InspectorId(), error,
        resource->GetResponse().EncodedDataLength(),
        ResourceLoadObserver::IsInternalRequest(
            resource->Options().initiator_info.name ==
            fetch_initiator_type_names::kInternal));
  }
}

void ResourceFetcher::MoveResourceLoaderToNonBlocking(ResourceLoader* loader) {
  DCHECK(loader);
  DCHECK(loaders_.Contains(loader));
  non_blocking_loaders_.insert(loader);
  loaders_.erase(loader);
}

bool ResourceFetcher::StartLoad(Resource* resource) {
  return StartLoad(resource, ResourceRequestBody(),
                   LoadBlockingPolicy::kDefault);
}

bool ResourceFetcher::StartLoad(Resource* resource,
                                ResourceRequestBody request_body,
                                LoadBlockingPolicy policy) {
  DCHECK(resource);
  DCHECK(resource->StillNeedsLoad());

  ResourceLoader* loader = nullptr;

  {
    // Forbids JavaScript/revalidation until start()
    // to prevent unintended state transitions.
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    if (properties_->ShouldBlockLoadingSubResource() && IsMainThread()) {
      GetMemoryCache()->Remove(resource);
      return false;
    }

    const ResourceRequestHead& request_head = resource->GetResourceRequest();

    if (resource_load_observer_) {
      DCHECK(!IsDetached());
      ResourceRequest request(request_head);
      request.SetHttpBody(request_body.FormBody());
      ResourceResponse response;
      resource_load_observer_->WillSendRequest(
          resource->InspectorId(), request, response, resource->GetType(),
          resource->Options().initiator_info);
    }

    using QuotaType = decltype(inflight_keepalive_bytes_);
    QuotaType size = 0;
    if (request_head.GetKeepalive() && request_body.FormBody()) {
      auto original_size = request_body.FormBody()->SizeInBytes();
      DCHECK_LE(inflight_keepalive_bytes_, kKeepaliveInflightBytesQuota);
      if (original_size > std::numeric_limits<QuotaType>::max())
        return false;
      size = static_cast<QuotaType>(original_size);
      if (kKeepaliveInflightBytesQuota - inflight_keepalive_bytes_ < size)
        return false;

      inflight_keepalive_bytes_ += size;
    }

    loader = MakeGarbageCollected<ResourceLoader>(
        this, scheduler_, resource, std::move(request_body), size);
    // Preload requests should not block the load event. IsLinkPreload()
    // actually continues to return true for Resources matched from the preload
    // cache that must block the load event, but that is OK because this method
    // is not responsible for promoting matched preloads to load-blocking. This
    // is handled by MakePreloadedResourceBlockOnloadIfNeeded().
    if (!resource->IsLinkPreload() &&
        resource->IsLoadEventBlockingResourceType() &&
        policy != LoadBlockingPolicy::kForceNonBlockingLoad) {
      loaders_.insert(loader);
    } else {
      non_blocking_loaders_.insert(loader);
    }
    resource->VirtualTimePauser().PauseVirtualTime();

    StorePerformanceTimingInitiatorInformation(resource);
  }

  loader->Start();

  {
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    // NotifyStartLoad() shouldn't cause AddClient/RemoveClient().
    Resource::ProhibitAddRemoveClientInScope
        prohibit_add_remove_client_in_scope(resource);
    if (!resource->IsLoaded())
      resource->NotifyStartLoad();
  }
  return true;
}

void ResourceFetcher::RemoveResourceLoader(ResourceLoader* loader) {
  DCHECK(loader);
  if (loaders_.Contains(loader))
    loaders_.erase(loader);
  else if (non_blocking_loaders_.Contains(loader))
    non_blocking_loaders_.erase(loader);
  else
    NOTREACHED();

  if (loaders_.IsEmpty() && non_blocking_loaders_.IsEmpty())
    keepalive_loaders_task_handle_.Cancel();
}

void ResourceFetcher::StopFetching() {
  StopFetchingInternal(StopFetchingTarget::kExcludingKeepaliveLoaders);
}

void ResourceFetcher::SetDefersLoading(bool defers) {
  for (const auto& loader : non_blocking_loaders_)
    loader->SetDefersLoading(defers);
  for (const auto& loader : loaders_)
    loader->SetDefersLoading(defers);
}

void ResourceFetcher::UpdateAllImageResourcePriorities() {
  TRACE_EVENT0(
      "blink",
      "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");

  HeapVector<Member<Resource>> to_be_removed;
#if DCHECK_IS_ON()
  DCHECK(!not_loaded_image_resources_is_being_iterated_);
  base::AutoReset<bool> is_being_modified_resetter(
      &not_loaded_image_resources_is_being_iterated_, true);
#endif
  for (Resource* resource : not_loaded_image_resources_) {
    DCHECK_EQ(resource->GetType(), ResourceType::kImage);
    if (resource->IsLoaded()) {
      to_be_removed.push_back(resource);
      continue;
    }

    if (!resource->IsLoading())
      continue;

    ResourcePriority resource_priority = resource->PriorityFromObservers();
    ResourceLoadPriority computed_load_priority = ComputeLoadPriority(
        ResourceType::kImage, resource->GetResourceRequest(),
        resource_priority.visibility);
    // Only boost the priority of an image, never lower it. This ensures that
    // there isn't priority churn if images move in and out of the viewport, or
    // are displayed more than once, both in and out of the viewport.
    if (computed_load_priority <= resource->GetResourceRequest().Priority())
      continue;

    DCHECK_GT(computed_load_priority,
              resource->GetResourceRequest().Priority());
    resource->DidChangePriority(computed_load_priority,
                                resource_priority.intra_priority_value);
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("network"), "ResourcePrioritySet",
        TRACE_ID_WITH_SCOPE("BlinkResourceID",
                            TRACE_ID_LOCAL(resource->InspectorId())),
        "data", ResourcePrioritySetData(computed_load_priority));
    DCHECK(!IsDetached());
    resource_load_observer_->DidChangePriority(
        resource->InspectorId(), computed_load_priority,
        resource_priority.intra_priority_value);
  }

  not_loaded_image_resources_.RemoveAll(to_be_removed);
  // Explicitly free the backing store to not regress memory.
  // TODO(bikineev): Revisit when young generation is done.
  to_be_removed.clear();
}

String ResourceFetcher::GetCacheIdentifier(const KURL& url) const {
  if (properties_->GetControllerServiceWorkerMode() !=
      mojom::ControllerServiceWorkerMode::kNoController) {
    return String::Number(properties_->ServiceWorkerId());
  }
  if (properties_->WebBundlePhysicalUrl().IsValid())
    return properties_->WebBundlePhysicalUrl().GetString();

  for (auto& bundle : subresource_web_bundles_) {
    if (bundle->CanHandleRequest(url))
      return bundle->GetCacheIdentifier();
  }

  return MemoryCache::DefaultCacheIdentifier();
}

void ResourceFetcher::EmulateLoadStartedForInspector(
    Resource* resource,
    const KURL& url,
    mojom::RequestContextType request_context,
    network::mojom::RequestDestination request_destination,
    const AtomicString& initiator_name) {
  base::AutoReset<bool> r(&is_in_request_resource_, true);
  if (CachedResource(url)) {
    return;
  }
  if (resource->ErrorOccurred()) {
    // We should ideally replay the error steps, but we cannot.
    return;
  }

  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(request_context);
  resource_request.SetRequestDestination(request_destination);
  if (!resource_request.PriorityHasBeenSet()) {
    resource_request.SetPriority(ComputeLoadPriority(
        resource->GetType(), resource_request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        false /* is_link_preload */));
  }
  resource_request.SetReferrerString(Referrer::NoReferrer());
  resource_request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  resource_request.SetInspectorId(CreateUniqueIdentifier());

  ResourceLoaderOptions options = resource->Options();
  options.initiator_info.name = initiator_name;
  FetchParameters params(std::move(resource_request), options);
  ResourceRequest last_resource_request(resource->LastResourceRequest());
  Context().CanRequest(resource->GetType(), last_resource_request,
                       last_resource_request.Url(), params.Options(),
                       ReportingDisposition::kReport,
                       last_resource_request.GetRedirectInfo());
  if (resource->GetStatus() == ResourceStatus::kNotStarted ||
      resource->GetStatus() == ResourceStatus::kPending) {
    // If the loading has not started, then we return here because loading
    // related events will be reported to the ResourceLoadObserver. If the
    // loading is ongoing, then we return here too because the loading
    // activity is merged.
    return;
  }
  DCHECK_EQ(resource->GetStatus(), ResourceStatus::kCached);
  DidLoadResourceFromMemoryCache(resource, params.GetResourceRequest(),
                                 false /* is_static_data */);
}

void ResourceFetcher::PrepareForLeakDetection() {
  // Stop loaders including keepalive ones that may persist after page
  // navigation and thus affect instance counters of leak detection.
  StopFetchingIncludingKeepaliveLoaders();
}

void ResourceFetcher::StopFetchingInternal(StopFetchingTarget target) {
  // TODO(toyoshim): May want to suspend scheduler while canceling loaders so
  // that the cancellations below do not awake unnecessary scheduling.

  HeapVector<Member<ResourceLoader>> loaders_to_cancel;
  for (const auto& loader : non_blocking_loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }
  for (const auto& loader : loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }

  for (const auto& loader : loaders_to_cancel) {
    if (loaders_.Contains(loader) || non_blocking_loaders_.Contains(loader))
      loader->Cancel();
  }
}

void ResourceFetcher::StopFetchingIncludingKeepaliveLoaders() {
  StopFetchingInternal(StopFetchingTarget::kIncludingKeepaliveLoaders);
}

void ResourceFetcher::ScheduleStaleRevalidate(Resource* stale_resource) {
  if (stale_resource->StaleRevalidationStarted())
    return;
  stale_resource->SetStaleRevalidationStarted();
  task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&ResourceFetcher::RevalidateStaleResource,
                WrapWeakPersistent(this), WrapPersistent(stale_resource)));
}

void ResourceFetcher::RevalidateStaleResource(Resource* stale_resource) {
  // Creating FetchParams from Resource::GetResourceRequest doesn't create
  // the exact same request as the original one, while for revalidation
  // purpose this is probably fine.
  // TODO(dtapuska): revisit this when we have a better way to re-dispatch
  // requests.
  ResourceRequest request;
  request.CopyHeadFrom(stale_resource->GetResourceRequest());
  FetchParameters params(std::move(request), nullptr /* world */);
  params.SetStaleRevalidation(true);
  params.MutableResourceRequest().SetSkipServiceWorker(true);
  // Stale revalidation resource requests should be very low regardless of
  // the |type|.
  params.MutableResourceRequest().SetPriority(ResourceLoadPriority::kVeryLow);
  RawResource::Fetch(
      params, this,
      MakeGarbageCollected<StaleRevalidationResourceClient>(stale_resource));
}

mojom::blink::BlobRegistry* ResourceFetcher::GetBlobRegistry() {
  if (!blob_registry_remote_.is_bound()) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        blob_registry_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return blob_registry_remote_.get();
}

FrameOrWorkerScheduler* ResourceFetcher::GetFrameOrWorkerScheduler() {
  return frame_or_worker_scheduler_.get();
}

void ResourceFetcher::AddSubresourceWebBundle(
    SubresourceWebBundle& subresource_web_bundle) {
  subresource_web_bundles_.insert(&subresource_web_bundle);
}

void ResourceFetcher::RemoveSubresourceWebBundle(
    SubresourceWebBundle& subresource_web_bundle) {
  subresource_web_bundles_.erase(&subresource_web_bundle);
}

void ResourceFetcher::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(properties_);
  visitor->Trace(resource_load_observer_);
  visitor->Trace(use_counter_);
  visitor->Trace(console_logger_);
  visitor->Trace(loader_factory_);
  visitor->Trace(scheduler_);
  visitor->Trace(archive_);
  visitor->Trace(loaders_);
  visitor->Trace(non_blocking_loaders_);
  visitor->Trace(cached_resources_map_);
  visitor->Trace(image_resources_);
  visitor->Trace(not_loaded_image_resources_);
  visitor->Trace(preloads_);
  visitor->Trace(matched_preloads_);
  visitor->Trace(resource_timing_info_map_);
  visitor->Trace(blob_registry_remote_);
  visitor->Trace(subresource_web_bundles_);
}

// static
const ResourceFetcher::ResourceFetcherSet&
ResourceFetcher::MainThreadFetchers() {
  return MainThreadFetchersSet();
}

}  // namespace blink
