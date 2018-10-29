/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_H_

#include <memory>

#include "services/network/public/cpp/cors/preflight_timing_info.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/preload_key.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/substitute_data.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class ArchiveResource;
class MHTMLArchive;
class KURL;
class Resource;
class ResourceTimingInfo;
enum class ResourceType : uint8_t;

// The ResourceFetcher provides a per-context interface to the MemoryCache and
// enforces a bunch of security checks and rules for resource revalidation. Its
// lifetime is roughly per-DocumentLoader, in that it is generally created in
// the DocumentLoader constructor and loses its ability to generate network
// requests when the DocumentLoader is destroyed.
//
// It is also created for workers and worklets.
//
// Documents also hold a pointer to ResourceFetcher for their lifetime (and will
// create one if they are initialized without a LocalFrame), so a Document can
// keep a ResourceFetcher alive past detach if scripts still reference the
// Document.
class PLATFORM_EXPORT ResourceFetcher
    : public GarbageCollectedFinalized<ResourceFetcher> {
  WTF_MAKE_NONCOPYABLE(ResourceFetcher);
  USING_PRE_FINALIZER(ResourceFetcher, ClearPreloads);

 public:
  static ResourceFetcher* Create(FetchContext* context) {
    return new ResourceFetcher(context);
  }
  virtual ~ResourceFetcher();
  virtual void Trace(blink::Visitor*);

  // Triggers a fetch based on the given FetchParameters (if there isn't a
  // suitable Resource already cached) and registers the given ResourceClient
  // with the Resource. Guaranteed to return a non-null Resource of the subtype
  // specified by ResourceFactory::GetType().
  Resource* RequestResource(FetchParameters&,
                            const ResourceFactory&,
                            ResourceClient*,
                            const SubstituteData& = SubstituteData());

  Resource* CachedResource(const KURL&) const;

  using DocumentResourceMap = HeapHashMap<String, WeakMember<Resource>>;
  const DocumentResourceMap& AllResources() const {
    return cached_resources_map_;
  }

  void HoldResourcesFromPreviousFetcher(ResourceFetcher*);
  void ClearResourcesFromPreviousFetcher();

  // Binds the given Resource instance to this ResourceFetcher instance to
  // start loading the Resource actually.
  // Usually, RequestResource() calls this method internally, but needs to
  // call this method explicitly on cases such as ResourceNeedsLoad() returning
  // false.
  bool StartLoad(Resource*);

  void SetAutoLoadImages(bool);
  void SetImagesEnabled(bool);

  FetchContext& Context() const;
  void ClearContext();

  int BlockingRequestCount() const;
  int NonblockingRequestCount() const;
  int ActiveRequestCount() const;

  enum ClearPreloadsPolicy {
    kClearAllPreloads,
    kClearSpeculativeMarkupPreloads
  };

  void EnableIsPreloadedForTest();
  bool IsPreloadedForTest(const KURL&) const;

  int CountPreloads() const { return preloads_.size(); }
  void ClearPreloads(ClearPreloadsPolicy = kClearAllPreloads);
  Vector<KURL> GetUrlsOfUnusedPreloads();

  MHTMLArchive* Archive() const { return archive_.Get(); }
  ArchiveResource* CreateArchive(Resource*);

  void SetDefersLoading(bool);
  void StopFetching();

  bool ShouldDeferImageLoad(const KURL&) const;

  void RecordResourceTimingOnRedirect(Resource*, const ResourceResponse&, bool);

  enum LoaderFinishType { kDidFinishLoading, kDidFinishFirstPartInMultipart };
  void HandleLoaderFinish(Resource*,
                          TimeTicks finish_time,
                          LoaderFinishType,
                          uint32_t inflight_keepalive_bytes,
                          bool should_report_corb_blocking,
                          const std::vector<network::cors::PreflightTimingInfo>&
                              cors_preflight_timing_info);
  void HandleLoaderError(Resource*,
                         const ResourceError&,
                         uint32_t inflight_keepalive_bytes);
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker() const;

  String GetCacheIdentifier() const;

  enum IsImageSet { kImageNotImageSet, kImageIsImageSet };

  WARN_UNUSED_RESULT static mojom::RequestContextType
  DetermineRequestContext(ResourceType, IsImageSet, bool is_main_frame);

  void UpdateAllImageResourcePriorities();

  void ReloadLoFiImages();

  // Calling this method before main document resource is fetched is invalid.
  ResourceTimingInfo* GetNavigationTimingInfo();

  // Returns whether the given resource is contained as a preloaded resource.
  bool ContainsAsPreload(Resource*) const;

  void RemovePreload(Resource*);

  void LoosenLoadThrottlingPolicy() { scheduler_->LoosenThrottlingPolicy(); }
  void OnNetworkQuiet();

  // Workaround for https://crbug.com/666214.
  // TODO(hiroshige): Remove this hack.
  void EmulateLoadStartedForInspector(Resource*,
                                      const KURL&,
                                      mojom::RequestContextType,
                                      const AtomicString& initiator_name);

  // This is called from leak detectors (Real-world leak detector & layout test
  // leak detector) to clean up loaders after page navigation before instance
  // counting.
  void PrepareForLeakDetection();

  void SetStaleWhileRevalidateEnabled(bool enabled);

  using ResourceFetcherSet = HeapHashSet<WeakMember<ResourceFetcher>>;
  static const ResourceFetcherSet& MainThreadFetchers();

 private:
  friend class ResourceCacheValidationSuppressor;
  enum class StopFetchingTarget {
    kExcludingKeepaliveLoaders,
    kIncludingKeepaliveLoaders,
  };

  ResourceFetcher(FetchContext*);

  void InitializeRevalidation(ResourceRequest&, Resource*);
  // When |security_origin| of the ResourceLoaderOptions is not a nullptr, it'll
  // be used instead of the associated FetchContext's SecurityOrigin.
  scoped_refptr<const SecurityOrigin> GetSourceOrigin(
      const ResourceLoaderOptions&) const;
  void AddToMemoryCacheIfNeeded(const FetchParameters&, Resource*);
  Resource* CreateResourceForLoading(const FetchParameters&,
                                     const ResourceFactory&);
  void StorePerformanceTimingInitiatorInformation(Resource*);
  ResourceLoadPriority ComputeLoadPriority(
      ResourceType,
      const ResourceRequest&,
      ResourcePriority::VisibilityStatus,
      FetchParameters::DeferOption = FetchParameters::kNoDefer,
      FetchParameters::SpeculativePreloadType =
          FetchParameters::SpeculativePreloadType::kNotSpeculative,
      bool is_link_preload = false);

  base::Optional<ResourceRequestBlockedReason> PrepareRequest(
      FetchParameters&,
      const ResourceFactory&,
      const SubstituteData&,
      unsigned long identifier);

  Resource* ResourceForStaticData(const FetchParameters&,
                                  const ResourceFactory&,
                                  const SubstituteData&);
  Resource* ResourceForBlockedRequest(const FetchParameters&,
                                      const ResourceFactory&,
                                      ResourceRequestBlockedReason,
                                      ResourceClient*);

  Resource* MatchPreload(const FetchParameters& params, ResourceType);
  void PrintPreloadWarning(Resource*, Resource::MatchStatus);
  void InsertAsPreloadIfNecessary(Resource*,
                                  const FetchParameters& params,
                                  ResourceType);

  bool IsImageResourceDisallowedToBeReused(const Resource&) const;

  void StopFetchingInternal(StopFetchingTarget);
  void StopFetchingIncludingKeepaliveLoaders();

  // RevalidationPolicy enum values are used in UMAs https://crbug.com/579496.
  enum RevalidationPolicy { kUse, kRevalidate, kReload, kLoad };

  // A wrapper just for placing a trace_event macro.
  RevalidationPolicy DetermineRevalidationPolicy(
      ResourceType,
      const FetchParameters&,
      const Resource& existing_resource,
      bool is_static_data) const;
  // Determines a RevalidationPolicy given a FetchParameters and an existing
  // resource retrieved from the memory cache (can be a newly constructed one
  // for a static data).
  RevalidationPolicy DetermineRevalidationPolicyInternal(
      ResourceType,
      const FetchParameters&,
      const Resource& existing_resource,
      bool is_static_data) const;

  void MakePreloadedResourceBlockOnloadIfNeeded(Resource*,
                                                const FetchParameters&);
  void MoveResourceLoaderToNonBlocking(ResourceLoader*);
  void RemoveResourceLoader(ResourceLoader*);
  void HandleLoadCompletion(Resource*);

  void RequestLoadStarted(unsigned long identifier,
                          Resource*,
                          const FetchParameters&,
                          RevalidationPolicy,
                          bool is_static_data = false);

  void DidLoadResourceFromMemoryCache(unsigned long identifier,
                                      Resource*,
                                      const ResourceRequest&);

  bool ResourceNeedsLoad(Resource*, const FetchParameters&, RevalidationPolicy);

  void ResourceTimingReportTimerFired(TimerBase*);

  void ReloadImagesIfNotDeferred();

  void UpdateMemoryCacheStats(Resource*,
                              RevalidationPolicy,
                              const FetchParameters&,
                              const ResourceFactory&,
                              bool is_static_data) const;

  void ScheduleStaleRevalidate(Resource* stale_resource);
  void RevalidateStaleResource(Resource* stale_resource);

  Member<FetchContext> context_;
  Member<ResourceLoadScheduler> scheduler_;

  DocumentResourceMap cached_resources_map_;
  HeapHashSet<WeakMember<Resource>> document_resources_;

  // When populated, forces Resources to remain alive across a navigation, to
  // increase the odds the next document will be able to reuse resources from
  // the previous page. Unpopulated unless experiment is enabled.
  HeapHashSet<Member<Resource>> resources_from_previous_fetcher_;

  HeapHashMap<PreloadKey, Member<Resource>> preloads_;
  HeapVector<Member<Resource>> matched_preloads_;
  Member<MHTMLArchive> archive_;

  TaskRunnerTimer<ResourceFetcher> resource_timing_report_timer_;

  using ResourceTimingInfoMap =
      HeapHashMap<Member<Resource>, scoped_refptr<ResourceTimingInfo>>;
  ResourceTimingInfoMap resource_timing_info_map_;

  scoped_refptr<ResourceTimingInfo> navigation_timing_info_;

  Vector<scoped_refptr<ResourceTimingInfo>> scheduled_resource_timing_reports_;

  HeapHashSet<Member<ResourceLoader>> loaders_;
  HeapHashSet<Member<ResourceLoader>> non_blocking_loaders_;

  std::unique_ptr<HashSet<String>> preloaded_urls_for_test_;

  // Timeout timer for keepalive requests.
  TaskHandle keepalive_loaders_task_handle_;

  uint32_t inflight_keepalive_bytes_ = 0;

  // 27 bits left
  bool auto_load_images_ : 1;
  bool images_enabled_ : 1;
  bool allow_stale_resources_ : 1;
  bool image_fetched_ : 1;
  bool stale_while_revalidate_enabled_ : 1;

  static constexpr uint32_t kKeepaliveInflightBytesQuota = 64 * 1024;
};

class ResourceCacheValidationSuppressor {
  WTF_MAKE_NONCOPYABLE(ResourceCacheValidationSuppressor);
  STACK_ALLOCATED();

 public:
  explicit ResourceCacheValidationSuppressor(ResourceFetcher* loader)
      : loader_(loader), previous_state_(false) {
    if (loader_) {
      previous_state_ = loader_->allow_stale_resources_;
      loader_->allow_stale_resources_ = true;
    }
  }
  ~ResourceCacheValidationSuppressor() {
    if (loader_)
      loader_->allow_stale_resources_ = previous_state_;
  }

 private:
  Member<ResourceFetcher> loader_;
  bool previous_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_H_
