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
#include <utility>

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/preload_key.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

enum class ResourceType : uint8_t;
class CodeCacheLoader;
class DetachableConsoleLogger;
class DetachableUseCounter;
class DetachableResourceFetcherProperties;
class FetchContext;
class FrameScheduler;
class MHTMLArchive;
class KURL;
class Resource;
class ResourceError;
class ResourceLoadObserver;
class ResourceTimingInfo;
class WebURLLoader;
struct ResourceFetcherInit;
struct ResourceLoaderOptions;

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
    : public GarbageCollected<ResourceFetcher> {
  USING_PRE_FINALIZER(ResourceFetcher, ClearPreloads);

 public:
  // An abstract interface for creating loaders.
  class LoaderFactory : public GarbageCollected<LoaderFactory> {
   public:
    virtual ~LoaderFactory() = default;

    virtual void Trace(Visitor*) {}

    // Create a WebURLLoader for given the request information and task runner.
    virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
        const ResourceRequest&,
        const ResourceLoaderOptions&,
        scoped_refptr<base::SingleThreadTaskRunner>) = 0;

    // Create a code cache loader to fetch data from code caches.
    virtual std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() = 0;
  };

  // ResourceFetcher creators are responsible for setting consistent objects
  // in ResourceFetcherInit to ensure correctness of this ResourceFetcher.
  explicit ResourceFetcher(const ResourceFetcherInit&);
  virtual ~ResourceFetcher();
  virtual void Trace(blink::Visitor*);

  // - This function returns the same object throughout this fetcher's
  //   entire life.
  // - The returned object remains valid after ClearContext() is called.
  const DetachableResourceFetcherProperties& GetProperties() const {
    return *properties_;
  }

  // Returns whether this fetcher is detached from the associated context.
  bool IsDetached() const;

  // Returns whether RequestResource or EmulateLoadStartedForInspector are being
  // called.
  bool IsInRequestResource() const { return is_in_request_resource_; }

  // Returns the observer object associated with this fetcher.
  ResourceLoadObserver* GetResourceLoadObserver() {
    // When detached, we must have a null observer.
    DCHECK(!IsDetached() || !resource_load_observer_);
    return resource_load_observer_;
  }
  // This must be called right after construction.
  void SetResourceLoadObserver(ResourceLoadObserver* observer) {
    DCHECK(!IsDetached());
    DCHECK(!resource_load_observer_);
    resource_load_observer_ = observer;
  }

  // Triggers a fetch based on the given FetchParameters (if there isn't a
  // suitable Resource already cached) and registers the given ResourceClient
  // with the Resource. Guaranteed to return a non-null Resource of the subtype
  // specified by ResourceFactory::GetType().
  Resource* RequestResource(FetchParameters&,
                            const ResourceFactory&,
                            ResourceClient*);

  // Returns the task runner used by this fetcher, and loading operations
  // this fetcher initiates. The returned task runner will keep working even
  // after ClearContext is called.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() const {
    return task_runner_;
  }

  // Create a loader. This cannot be called after ClearContext is called.
  std::unique_ptr<WebURLLoader> CreateURLLoader(const ResourceRequest&,
                                                const ResourceLoaderOptions&);
  // Create a code cache loader. This cannot be called after ClearContext is
  // called.
  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader();

  Resource* CachedResource(const KURL&) const;
  static void AddPriorityObserverForTesting(const KURL&,
                                            base::OnceCallback<void(int)>);

  using DocumentResourceMap = HeapHashMap<String, WeakMember<Resource>>;
  const DocumentResourceMap& AllResources() const {
    return cached_resources_map_;
  }

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
  DetachableUseCounter& GetUseCounter() { return *use_counter_; }
  DetachableConsoleLogger& GetConsoleLogger() { return *console_logger_; }

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

  void SetDefersLoading(bool);
  void StopFetching();

  bool ShouldDeferImageLoad(const KURL&) const;

  void RecordResourceTimingOnRedirect(Resource*,
                                      const ResourceResponse&,
                                      const KURL& new_url);

  enum LoaderFinishType { kDidFinishLoading, kDidFinishFirstPartInMultipart };
  void HandleLoaderFinish(Resource*,
                          base::TimeTicks finish_time,
                          LoaderFinishType,
                          uint32_t inflight_keepalive_bytes,
                          bool should_report_corb_blocking);
  void HandleLoaderError(Resource*,
                         const ResourceError&,
                         uint32_t inflight_keepalive_bytes);
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker() const;

  String GetCacheIdentifier() const;

  enum IsImageSet { kImageNotImageSet, kImageIsImageSet };

  WARN_UNUSED_RESULT static mojom::RequestContextType DetermineRequestContext(
      ResourceType,
      IsImageSet);

  void UpdateAllImageResourcePriorities();

  // Returns whether the given resource is contained as a preloaded resource.
  bool ContainsAsPreload(Resource*) const;

  void RemovePreload(Resource*);

  void LoosenLoadThrottlingPolicy() { scheduler_->LoosenThrottlingPolicy(); }

  // Workaround for https://crbug.com/666214.
  // TODO(hiroshige): Remove this hack.
  void EmulateLoadStartedForInspector(Resource*,
                                      const KURL&,
                                      mojom::RequestContextType,
                                      const AtomicString& initiator_name);

  // This is called from leak detectors (Real-world leak detector & web test
  // leak detector) to clean up loaders after page navigation before instance
  // counting.
  void PrepareForLeakDetection();

  using ResourceFetcherSet = HeapHashSet<WeakMember<ResourceFetcher>>;
  static const ResourceFetcherSet& MainThreadFetchers();

  mojom::blink::BlobRegistry* GetBlobRegistry();

  FrameScheduler* GetFrameScheduler();

  ResourceLoadPriority ComputeLoadPriorityForTesting(
      ResourceType type,
      const ResourceRequest& request,
      ResourcePriority::VisibilityStatus visibility_statue,
      FetchParameters::DeferOption defer_option,
      FetchParameters::SpeculativePreloadType speculative_preload_type,
      bool is_link_preload) {
    return ComputeLoadPriority(type, request, visibility_statue, defer_option,
                               speculative_preload_type, is_link_preload);
  }

  void SetShouldLogRequestAsInvalidInImportedDocument() {
    should_log_request_as_invalid_in_imported_document_ = true;
  }

 private:
  friend class ResourceCacheValidationSuppressor;
  enum class StopFetchingTarget {
    kExcludingKeepaliveLoaders,
    kIncludingKeepaliveLoaders,
  };

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

  // |virtual_time_pauser| is an output parameter. PrepareRequest may
  // create a new WebScopedVirtualTimePauser and set it to
  // |virtual_time_pauser|.
  base::Optional<ResourceRequestBlockedReason> PrepareRequest(
      FetchParameters&,
      const ResourceFactory&,
      uint64_t identifier,
      WebScopedVirtualTimePauser& virtual_time_pauser);

  Resource* ResourceForStaticData(const FetchParameters&,
                                  const ResourceFactory&);
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
  static const char* GetNameFor(RevalidationPolicy policy);
  // Determines a RevalidationPolicy given a FetchParameters and an existing
  // resource retrieved from the memory cache (can be a newly constructed one
  // for a static data).
  // The first member of the returned value is the revalidation policy. The
  // second member of the returned value is a string explaining the reason for
  // trace events. Its extent is unbound.
  std::pair<RevalidationPolicy, const char*>
  DetermineRevalidationPolicyInternal(ResourceType,
                                      const FetchParameters&,
                                      const Resource& existing_resource,
                                      bool is_static_data) const;

  void MakePreloadedResourceBlockOnloadIfNeeded(Resource*,
                                                const FetchParameters&);
  void MoveResourceLoaderToNonBlocking(ResourceLoader*);
  void RemoveResourceLoader(ResourceLoader*);

  void DidLoadResourceFromMemoryCache(uint64_t identifier,
                                      Resource*,
                                      const ResourceRequest&,
                                      bool is_static_data);

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

  Member<DetachableResourceFetcherProperties> properties_;
  Member<ResourceLoadObserver> resource_load_observer_;
  Member<FetchContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const Member<DetachableUseCounter> use_counter_;
  const Member<DetachableConsoleLogger> console_logger_;
  Member<LoaderFactory> loader_factory_;
  const Member<ResourceLoadScheduler> scheduler_;

  DocumentResourceMap cached_resources_map_;

  // |image_resources_| is the subset of all image resources for the document.
  HeapHashSet<WeakMember<Resource>> image_resources_;

  // |not_loaded_image_resources_| is a subset of |image_resources_| where
  // |Resource::IsLoaded| might be false. The is used for performance
  // optimizations and might still contain images which are actually loaded.
  HeapHashSet<WeakMember<Resource>> not_loaded_image_resources_;

  HeapHashMap<PreloadKey, Member<Resource>> preloads_;
  HeapVector<Member<Resource>> matched_preloads_;
  Member<MHTMLArchive> archive_;

  TaskRunnerTimer<ResourceFetcher> resource_timing_report_timer_;

  using ResourceTimingInfoMap =
      HeapHashMap<Member<Resource>, scoped_refptr<ResourceTimingInfo>>;
  ResourceTimingInfoMap resource_timing_info_map_;

  Vector<scoped_refptr<ResourceTimingInfo>> scheduled_resource_timing_reports_;

  HeapHashSet<Member<ResourceLoader>> loaders_;
  HeapHashSet<Member<ResourceLoader>> non_blocking_loaders_;

  std::unique_ptr<HashSet<String>> preloaded_urls_for_test_;

  // TODO(altimin): Move FrameScheduler to oilpan.
  base::WeakPtr<FrameScheduler> frame_scheduler_;

  // Timeout timer for keepalive requests.
  TaskHandle keepalive_loaders_task_handle_;

  uint32_t inflight_keepalive_bytes_ = 0;

  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;

  // This is not in the bit field below because we want to use AutoReset.
  bool is_in_request_resource_ = false;

  // 26 bits left
  bool auto_load_images_ : 1;
  bool images_enabled_ : 1;
  bool allow_stale_resources_ : 1;
  bool image_fetched_ : 1;
  bool stale_while_revalidate_enabled_ : 1;
  // for https://crbug.com/961614
  bool should_log_request_as_invalid_in_imported_document_ : 1;

  static constexpr uint32_t kKeepaliveInflightBytesQuota = 64 * 1024;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcher);
};

class ResourceCacheValidationSuppressor {
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

  DISALLOW_COPY_AND_ASSIGN(ResourceCacheValidationSuppressor);
};

// Used for ResourceFetcher construction.
// Creators of ResourceFetcherInit are responsible for setting consistent
// members to ensure the correctness of ResourceFetcher.
struct PLATFORM_EXPORT ResourceFetcherInit final {
  STACK_ALLOCATED();

 public:
  // |context| and |task_runner| must not be null.
  // |loader_factory| can be null if |properties.IsDetached()| is true.
  ResourceFetcherInit(DetachableResourceFetcherProperties& properties,
                      FetchContext* context,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      ResourceFetcher::LoaderFactory* loader_factory);

  const Member<DetachableResourceFetcherProperties> properties;
  const Member<FetchContext> context;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  const Member<ResourceFetcher::LoaderFactory> loader_factory;
  Member<DetachableUseCounter> use_counter;
  Member<DetachableConsoleLogger> console_logger;
  ResourceLoadScheduler::ThrottlingPolicy initial_throttling_policy =
      ResourceLoadScheduler::ThrottlingPolicy::kNormal;
  Member<MHTMLArchive> archive;
  FrameScheduler* frame_scheduler = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherInit);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_FETCHER_H_
