/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.

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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_H_

#include <memory>
#include "base/auto_reset.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-shared.h"
#include "third_party/blink/public/platform/web_data_consumer_handle.h"
#include "third_party/blink/public/platform/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class FetchParameters;
class ResourceClient;
class ResourceFetcher;
class ResourceFinishObserver;
class ResourceTimingInfo;
class ResourceLoader;
class SecurityOrigin;

// |ResourceType| enum values are used in UMAs, so do not change the values of
// existing types. When adding a new type, append it at the end.
enum class ResourceType : uint8_t {
  kMainResource,
  kImage,
  kCSSStyleSheet,
  kScript,
  kFont,
  kRaw,
  kSVGDocument,
  kXSLStyleSheet,
  kLinkPrefetch,
  kTextTrack,
  kImportResource,
  kAudio,
  kVideo,
  kManifest,
  kMock  // Only for testing
};

// A callback for sending the serialized data of cached metadata back to the
// platform.
class CachedMetadataSender {
 public:
  virtual ~CachedMetadataSender() = default;
  virtual void Send(const char*, size_t) = 0;

  // IsServedFromCacheStorage is used to alter caching strategy to be more
  // aggressive. See ScriptController.cpp CacheOptions() for an example.
  virtual bool IsServedFromCacheStorage() = 0;
};

// A resource that is held in the cache. Classes who want to use this object
// should derive from ResourceClient, to get the function calls in case the
// requested data has arrived. This class also does the actual communication
// with the loader to obtain the resource from the network.
class PLATFORM_EXPORT Resource : public GarbageCollectedFinalized<Resource>,
                                 public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(Resource);
  WTF_MAKE_NONCOPYABLE(Resource);

 public:
  // An enum representing whether a resource match with another resource.
  // There are three kinds of status.
  // - kOk, which represents the success.
  // - kUnknownFailure, which represents miscellaneous failures. This includes
  //   failures which cannot happen for preload matching (for example,
  //   a failure due to non-cacheable request method cannot be happen for
  //   preload matching).
  // - other specific error status
  enum class MatchStatus {
    // Match succeeds.
    kOk,

    // Match fails because of an unknown reason.
    kUnknownFailure,

    // Subresource integrity value doesn't match.
    kIntegrityMismatch,

    // Match fails because the new request wants to load the content
    // as a blob.
    kBlobRequest,

    // Match fails because loading image is disabled.
    kImageLoadingDisabled,

    // Match fails due to different synchronous flags.
    kSynchronousFlagDoesNotMatch,

    // Match fails due to different request modes.
    kRequestModeDoesNotMatch,

    // Match fails due to different request credentials modes.
    kRequestCredentialsModeDoesNotMatch,

    // Match fails because keepalive flag is set on either requests.
    kKeepaliveSet,

    // Match fails due to different request methods.
    kRequestMethodDoesNotMatch,

    // Match fails due to different request headers.
    kRequestHeadersDoNotMatch,

    // Match fails due to different image placeholder policies.
    kImagePlaceholder,
  };

  // Used by reloadIfLoFiOrPlaceholderImage().
  enum ReloadLoFiOrPlaceholderPolicy {
    kReloadIfNeeded,
    kReloadAlways,
  };

  ~Resource() override;

  void Trace(blink::Visitor*) override;

  virtual WTF::TextEncoding Encoding() const { return WTF::TextEncoding(); }
  virtual void AppendData(const char*, size_t);
  virtual void FinishAsError(const ResourceError&,
                             base::SingleThreadTaskRunner*);

  void SetLinkPreload(bool is_link_preload) { link_preload_ = is_link_preload; }
  bool IsLinkPreload() const { return link_preload_; }

  const ResourceError& GetResourceError() const {
    DCHECK(error_);
    return *error_;
  }

  void SetIdentifier(unsigned long identifier) { identifier_ = identifier; }
  unsigned long Identifier() const { return identifier_; }

  virtual bool ShouldIgnoreHTTPStatusCodeErrors() const { return false; }

  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }
  const ResourceRequest& LastResourceRequest() const;

  virtual void SetRevalidatingRequest(const ResourceRequest&);

  // This url can have a fragment, but it can match resources that differ by the
  // fragment only.
  const KURL& Url() const { return GetResourceRequest().Url(); }
  ResourceType GetType() const { return static_cast<ResourceType>(type_); }
  const ResourceLoaderOptions& Options() const { return options_; }
  ResourceLoaderOptions& MutableOptions() { return options_; }

  void DidChangePriority(ResourceLoadPriority, int intra_priority_value);
  virtual ResourcePriority PriorityFromObservers() {
    return ResourcePriority();
  }

  // If this Resource is already finished when AddClient is called, the
  // ResourceClient will be notified asynchronously by a task scheduled
  // on the given base::SingleThreadTaskRunner. Otherwise, the given
  // base::SingleThreadTaskRunner is unused.
  void AddClient(ResourceClient*, base::SingleThreadTaskRunner*);
  void RemoveClient(ResourceClient*);

  // If this Resource is already finished when AddFinishObserver is called, the
  // ResourceFinishObserver will be notified asynchronously by a task scheduled
  // on the given base::SingleThreadTaskRunner. Otherwise, the given
  // base::SingleThreadTaskRunner is unused.
  void AddFinishObserver(ResourceFinishObserver*,
                         base::SingleThreadTaskRunner*);
  void RemoveFinishObserver(ResourceFinishObserver*);

  bool IsUnusedPreload() const { return is_unused_preload_; }

  ResourceStatus GetStatus() const { return status_; }
  void SetStatus(ResourceStatus status) { status_ = status; }

  size_t size() const { return EncodedSize() + DecodedSize() + OverheadSize(); }

  // Returns the size of content (response body) before decoding. Adding a new
  // usage of this function is not recommended (See the TODO below).
  //
  // TODO(hiroshige): Now EncodedSize/DecodedSize states are inconsistent and
  // need to be refactored (crbug/643135).
  size_t EncodedSize() const { return encoded_size_; }

  // Returns the current memory usage for the encoded data. Adding a new usage
  // of this function is not recommended as the same reason as |EncodedSize()|.
  //
  // |EncodedSize()| and |EncodedSizeMemoryUsageForTesting()| can return
  // different values, e.g., when ImageResource purges encoded image data after
  // finishing loading.
  size_t EncodedSizeMemoryUsageForTesting() const {
    return encoded_size_memory_usage_;
  }

  size_t DecodedSize() const { return decoded_size_; }
  size_t OverheadSize() const { return overhead_size_; }

  bool IsLoaded() const { return status_ > ResourceStatus::kPending; }

  bool IsLoading() const { return status_ == ResourceStatus::kPending; }
  bool StillNeedsLoad() const { return status_ < ResourceStatus::kPending; }

  void SetLoader(ResourceLoader*);
  ResourceLoader* Loader() const { return loader_.Get(); }

  bool ShouldBlockLoadEvent() const;
  bool IsLoadEventBlockingResourceType() const;

  // Computes the status of an object after loading. Updates the expire date on
  // the cache entry file
  virtual void Finish(TimeTicks finish_time, base::SingleThreadTaskRunner*);
  void FinishForTest() { Finish(TimeTicks(), nullptr); }

  virtual scoped_refptr<const SharedBuffer> ResourceBuffer() const {
    return data_;
  }
  void SetResourceBuffer(scoped_refptr<SharedBuffer>);

  virtual bool WillFollowRedirect(const ResourceRequest&,
                                  const ResourceResponse&);

  // Called when a redirect response was received but a decision has already
  // been made to not follow it.
  virtual void WillNotFollowRedirect() {}

  virtual void ResponseReceived(const ResourceResponse&,
                                std::unique_ptr<WebDataConsumerHandle>);
  void SetResponse(const ResourceResponse&);
  const ResourceResponse& GetResponse() const { return response_; }

  virtual void ReportResourceTimingToClients(const ResourceTimingInfo&) {}

  // Sets the serialized metadata retrieved from the platform's cache.
  // Subclasses of Resource that support cached metadata should override this
  // method with one that fills the current CachedMetadataHandler.
  virtual void SetSerializedCachedMetadata(const char*, size_t);

  AtomicString HttpContentType() const;

  bool WasCanceled() const { return error_ && error_->IsCancellation(); }
  bool ErrorOccurred() const {
    return status_ == ResourceStatus::kLoadError ||
           status_ == ResourceStatus::kDecodeError;
  }
  bool LoadFailedOrCanceled() const { return !!error_; }

  DataBufferingPolicy GetDataBufferingPolicy() const {
    return options_.data_buffering_policy;
  }
  void SetDataBufferingPolicy(DataBufferingPolicy);

  void MarkAsPreload();
  // Returns true if |this| resource is matched with the given parameters.
  virtual bool MatchPreload(const FetchParameters&,
                            base::SingleThreadTaskRunner*);

  bool CanReuseRedirectChain() const;
  bool MustRevalidateDueToCacheHeaders(bool allow_stale) const;
  bool ShouldRevalidateStaleResponse() const;
  virtual bool CanUseCacheValidator() const;
  bool IsCacheValidator() const { return is_revalidating_; }
  bool HasCacheControlNoStoreHeader() const;
  bool MustReloadDueToVaryHeader(const ResourceRequest& new_request) const;

  // Returns true if any response returned from the upstream in the redirect
  // chain is stale and requires triggering async stale revalidation. Once
  // revalidation is started SetStaleRevalidationStarted() should be called.
  bool StaleRevalidationRequested() const;

  // Set that stale revalidation has been started so that subsequent
  // requests won't trigger it again. When stale revalidation is completed
  // this resource will be removed from the MemoryCache so there is no
  // need to reset it back to false.
  bool StaleRevalidationStarted() const { return stale_revalidation_started_; }
  void SetStaleRevalidationStarted() { stale_revalidation_started_ = true; }

  const IntegrityMetadataSet& IntegrityMetadata() const {
    return options_.integrity_metadata;
  }
  ResourceIntegrityDisposition IntegrityDisposition() const {
    return integrity_disposition_;
  }
  const SubresourceIntegrity::ReportInfo& IntegrityReportInfo() const {
    return integrity_report_info_;
  }
  bool MustRefetchDueToIntegrityMetadata(const FetchParameters&) const;

  bool IsAlive() const { return is_alive_; }

  void SetCacheIdentifier(const String& cache_identifier) {
    cache_identifier_ = cache_identifier;
  }
  String CacheIdentifier() const { return cache_identifier_; }

  // https://fetch.spec.whatwg.org/#concept-request-origin
  const scoped_refptr<const SecurityOrigin>& GetOrigin() const;

  virtual void DidSendData(unsigned long long /* bytesSent */,
                           unsigned long long /* totalBytesToBeSent */) {}
  virtual void DidDownloadData(int) {}
  virtual void DidDownloadToBlob(scoped_refptr<BlobDataHandle>) {}

  TimeTicks LoadFinishTime() const { return load_finish_time_; }

  void SetEncodedDataLength(int64_t value) {
    response_.SetEncodedDataLength(value);
  }
  void SetEncodedBodyLength(int value) {
    response_.SetEncodedBodyLength(value);
  }
  void SetDecodedBodyLength(int value) {
    response_.SetDecodedBodyLength(value);
  }

  // Returns |kOk| when |this| can be resused for the given arguments.
  virtual MatchStatus CanReuse(const FetchParameters& params) const;

  // TODO(yhirano): Remove this once out-of-blink CORS is fully enabled.
  void SetResponseType(network::mojom::FetchResponseType response_type) {
    response_.SetType(response_type);
  }

  // If cache-aware loading is activated, this callback is called when the first
  // disk-cache-only request failed due to cache miss. After this callback,
  // cache-aware loading is deactivated and a reload with original request will
  // be triggered right away in ResourceLoader.
  virtual void WillReloadAfterDiskCacheMiss() {}

  // TODO(shaochuan): This is for saving back the actual ResourceRequest sent
  // in ResourceFetcher::StartLoad() for retry in cache-aware loading, remove
  // once ResourceRequest is not modified in StartLoad(). crbug.com/632580
  void SetResourceRequest(const ResourceRequest& resource_request) {
    resource_request_ = resource_request;
  }

  // Used by the MemoryCache to reduce the memory consumption of the entry.
  void Prune();

  virtual void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                            WebProcessMemoryDump*) const;

  // If this Resource is ImageResource and has the Lo-Fi response headers or is
  // a placeholder, reload the full original image with the Lo-Fi state set to
  // off and optionally bypassing the cache.
  virtual void ReloadIfLoFiOrPlaceholderImage(ResourceFetcher*,
                                              ReloadLoFiOrPlaceholderPolicy) {}

  // Used to notify ImageResourceContent of the start of actual loading.
  // JavaScript calls or client/observer notifications are disallowed inside
  // NotifyStartLoad().
  virtual void NotifyStartLoad() {
    CHECK_EQ(status_, ResourceStatus::kNotStarted);
    status_ = ResourceStatus::kPending;
  }

  static const char* ResourceTypeToString(
      ResourceType,
      const AtomicString& fetch_initiator_name);

  static blink::mojom::CodeCacheType ResourceTypeToCodeCacheType(ResourceType);

  class ProhibitAddRemoveClientInScope : public base::AutoReset<bool> {
   public:
    ProhibitAddRemoveClientInScope(Resource* resource)
        : AutoReset(&resource->is_add_remove_client_prohibited_, true) {}
  };

  class RevalidationStartForbiddenScope : public base::AutoReset<bool> {
   public:
    RevalidationStartForbiddenScope(Resource* resource)
        : AutoReset(&resource->is_revalidation_start_forbidden_, true) {}
  };

  WebScopedVirtualTimePauser& VirtualTimePauser() {
    return virtual_time_pauser_;
  }

 protected:
  Resource(const ResourceRequest&, ResourceType, const ResourceLoaderOptions&);

  virtual void NotifyFinished();

  void MarkClientFinished(ResourceClient*);

  virtual bool HasClientsOrObservers() const {
    return !clients_.IsEmpty() || !clients_awaiting_callback_.IsEmpty() ||
           !finished_clients_.IsEmpty() || !finish_observers_.IsEmpty();
  }
  virtual void DestroyDecodedDataForFailedRevalidation() {}

  void SetEncodedSize(size_t);
  void SetDecodedSize(size_t);

  void FinishPendingClients();

  virtual void DidAddClient(ResourceClient*);
  void WillAddClientOrObserver();

  void DidRemoveClientOrObserver();
  virtual void AllClientsAndObserversRemoved();

  bool HasClient(ResourceClient* client) const {
    return clients_.Contains(client) ||
           clients_awaiting_callback_.Contains(client) ||
           finished_clients_.Contains(client);
  }

  struct RedirectPair {
    DISALLOW_NEW();

   public:
    explicit RedirectPair(const ResourceRequest& request,
                          const ResourceResponse& redirect_response)
        : request_(request), redirect_response_(redirect_response) {}

    ResourceRequest request_;
    ResourceResponse redirect_response_;
  };
  const Vector<RedirectPair>& RedirectChain() const { return redirect_chain_; }

  virtual void DestroyDecodedDataIfPossible() {}

  // Returns the memory dump name used for tracing. See Resource::OnMemoryDump.
  String GetMemoryDumpName() const;

  const HeapHashCountedSet<WeakMember<ResourceClient>>& Clients() const {
    return clients_;
  }

  void SetCachePolicyBypassingCache();
  void SetPreviewsState(WebURLRequest::PreviewsState);
  void ClearRangeRequestHeader();

  SharedBuffer* Data() const { return data_.get(); }
  void ClearData();

  virtual void SetEncoding(const String&) {}

  // Create a handler for the cached metadata of this resource. Subclasses of
  // Resource that support cached metadata should override this method with one
  // that creates an appropriate CachedMetadataHandler implementation, and
  // override SetSerializedCachedMetadata with an implementation that fills the
  // cache handler.
  virtual CachedMetadataHandler* CreateCachedMetadataHandler(
      std::unique_ptr<CachedMetadataSender> send_callback) {
    return nullptr;
  }

  CachedMetadataHandler* CacheHandler() { return cache_handler_.Get(); }

 private:
  friend class ResourceLoader;

  void RevalidationSucceeded(const ResourceResponse&);
  void RevalidationFailed();

  size_t CalculateOverheadSize() const;

  String ReasonNotDeletable() const;

  // MemoryCoordinatorClient overrides:
  void OnPurgeMemory() override;

  void CheckResourceIntegrity();
  void TriggerNotificationForFinishObservers(base::SingleThreadTaskRunner*);

  // Helper for creating the send callback function for the cached metadata
  // handler.
  std::unique_ptr<CachedMetadataSender> CreateCachedMetadataSender() const;

  ResourceType type_;
  ResourceStatus status_;

  Member<CachedMetadataHandler> cache_handler_;

  base::Optional<ResourceError> error_;

  TimeTicks load_finish_time_;

  unsigned long identifier_;

  size_t encoded_size_;
  size_t encoded_size_memory_usage_;
  size_t decoded_size_;

  // Resource::CalculateOverheadSize() is affected by changes in
  // |m_resourceRequest.url()|, but |m_overheadSize| is not updated after
  // initial |m_resourceRequest| is given, to reduce MemoryCache manipulation
  // and thus potential bugs. crbug.com/594644
  const size_t overhead_size_;

  String cache_identifier_;

  bool link_preload_;
  bool is_revalidating_;
  bool is_alive_;
  bool is_add_remove_client_prohibited_;
  bool is_revalidation_start_forbidden_ = false;
  bool is_unused_preload_ = false;
  bool stale_revalidation_started_ = false;

  ResourceIntegrityDisposition integrity_disposition_;
  SubresourceIntegrity::ReportInfo integrity_report_info_;

  // Ordered list of all redirects followed while fetching this resource.
  Vector<RedirectPair> redirect_chain_;

  HeapHashCountedSet<WeakMember<ResourceClient>> clients_;
  HeapHashCountedSet<WeakMember<ResourceClient>> clients_awaiting_callback_;
  HeapHashCountedSet<WeakMember<ResourceClient>> finished_clients_;
  HeapHashSet<WeakMember<ResourceFinishObserver>> finish_observers_;

  ResourceLoaderOptions options_;

  double response_timestamp_;

  TaskHandle async_finish_pending_clients_task_;

  ResourceRequest resource_request_;
  Member<ResourceLoader> loader_;
  ResourceResponse response_;

  scoped_refptr<SharedBuffer> data_;

  WebScopedVirtualTimePauser virtual_time_pauser_;
};

class ResourceFactory {
  STACK_ALLOCATED();

 public:
  virtual Resource* Create(const ResourceRequest&,
                           const ResourceLoaderOptions&,
                           const TextResourceDecoderOptions&) const = 0;

  ResourceType GetType() const { return type_; }
  TextResourceDecoderOptions::ContentType ContentType() const {
    return content_type_;
  }

 protected:
  explicit ResourceFactory(ResourceType type,
                           TextResourceDecoderOptions::ContentType content_type)
      : type_(type), content_type_(content_type) {}

  ResourceType type_;
  TextResourceDecoderOptions::ContentType content_type_;
};

class NonTextResourceFactory : public ResourceFactory {
 protected:
  explicit NonTextResourceFactory(ResourceType type)
      : ResourceFactory(type, TextResourceDecoderOptions::kPlainTextContent) {}

  virtual Resource* Create(const ResourceRequest&,
                           const ResourceLoaderOptions&) const = 0;

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const TextResourceDecoderOptions&) const final {
    return Create(request, options);
  }
};

#define DEFINE_RESOURCE_TYPE_CASTS(typeName)                          \
  DEFINE_TYPE_CASTS(typeName##Resource, Resource, resource,           \
                    resource->GetType() == ResourceType::k##typeName, \
                    resource.GetType() == ResourceType::k##typeName);

}  // namespace blink

#endif
