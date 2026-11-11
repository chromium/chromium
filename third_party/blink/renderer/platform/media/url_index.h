// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_URL_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_URL_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "media/base/data_source.h"
#include "third_party/blink/renderer/platform/media/multi_buffer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

const int64_t kPositionNotSpecified = -1;

class ResourceFetchContext;
class UrlData;

// A multibuffer for loading media resources which knows
// how to create MultiBufferDataProviders to load data
// into the cache.
class PLATFORM_EXPORT ResourceMultiBuffer : public MultiBuffer {
 public:
  ResourceMultiBuffer(UrlData* url_data_,
                      int block_shift,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ResourceMultiBuffer() override;

  // MultiBuffer implementation.
  std::unique_ptr<MultiBuffer::DataProvider> CreateWriter(
      const BlockId& pos,
      bool is_client_audio_element) override;
  bool RangeSupported() const override;
  void OnEmpty() override;

 protected:
  // Do not access from destructor, it is a pointer to the
  // object that contains us.
  raw_ptr<UrlData> url_data_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class UrlIndex;

// All the data & metadata for a single resource.
// Data is cached using a MultiBuffer instance.
class PLATFORM_EXPORT UrlData : public RefCounted<UrlData> {
 public:
  // Keep in sync with WebMediaPlayer::CorsMode.
  enum CorsMode { CORS_UNSPECIFIED, CORS_ANONYMOUS, CORS_USE_CREDENTIALS };
  enum CacheMode { kNormal, kCacheDisabled };
  enum class ServiceWorkerMode {
    kAllow,
    kSkip,
  };

  struct Key {
    Key();
    Key(const KURL& url,
        scoped_refptr<const SecurityOrigin> security_origin,
        CorsMode cors_mode,
        media::DataSource::EncodingMode encoding_mode,
        ServiceWorkerMode service_worker_mode = ServiceWorkerMode::kAllow);
    Key(const Key&);
    Key(Key&&);
    Key& operator=(const Key&);
    Key& operator=(Key&&);
    ~Key();

    KURL url;
    scoped_refptr<const SecurityOrigin> security_origin;
    CorsMode cors_mode = CORS_UNSPECIFIED;
    media::DataSource::EncodingMode encoding_mode =
        media::DataSource::EncodingMode::kIdentity;
    ServiceWorkerMode service_worker_mode = ServiceWorkerMode::kAllow;
  };

  // `url_index` is a WeakPtr since while UrlData objects are created by the
  // UrlIndex they are not owned by the UrlIndex until after the network load
  // starts successfully. If the UrlIndex dies before that happens the UrlData
  // is left with a dangling pointer to the index.
  UrlData(base::PassKey<UrlIndex>,
          const KURL& url,
          CorsMode cors_mode,
          CacheMode cache_lookup_mode,
          scoped_refptr<const SecurityOrigin> security_origin,
          media::DataSource::EncodingMode encoding_mode,
          ServiceWorkerMode service_worker_mode,
          const KURL& original_request_url,
          base::WeakPtr<UrlIndex> url_index,
          scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UrlData(const UrlData&) = delete;
  UrlData& operator=(const UrlData&) = delete;

  // Accessors
  const KURL& url() const { return url_; }

  // The URL to issue network range requests against. Equal to `url()` except
  // for unindexed bounce-back redirects, where follow-up range requests replay
  // from `original_request_url_` to preserve `net::URLRequest::url_chain()`.
  const KURL& request_url() const {
    return original_request_url_.IsEmpty() ? url_ : original_request_url_;
  }
  bool has_original_request_url() const {
    return !original_request_url_.IsEmpty();
  }

  ServiceWorkerMode service_worker_mode() const { return service_worker_mode_; }

  const scoped_refptr<const SecurityOrigin>& security_origin() const {
    return security_origin_;
  }

  const std::optional<KURL>& data_origin() const { return data_origin_; }

  // Cross-origin access mode
  CorsMode cors_mode() const { return cors_mode_; }

  bool has_access_control() const { return has_access_control_; }

  bool passed_timing_allow_origin_check() const {
    return passed_timing_allow_origin_check_.value_or(false);
  }

  const std::string& mime_type() const { return mime_type_; }

  // Are HTTP range requests supported?
  bool range_supported() const { return range_supported_; }

  // True if we found a reason why this URL won't be stored in the
  // HTTP disk cache.
  bool cacheable() const { return cacheable_; }

  // True if this UrlData and any it might redirect to should bypass cache
  // lookups, regardless of disk cache or response status.
  CacheMode cache_lookup_mode() const { return cache_lookup_mode_; }

  media::DataSource::EncodingMode encoding_mode() const {
    return encoding_mode_;
  }

  // Last used time.
  base::Time last_used() const { return last_used_; }

  // Last modified time.
  base::Time last_modified() const { return last_modified_; }

  const std::string& etag() const { return etag_; }

  // Expiration time.
  base::Time valid_until() const { return valid_until_; }

  // The key used by UrlIndex to find this UrlData.
  Key key() const;

  // Length of data associated with url or |kPositionNotSpecified|
  int64_t length() const { return length_; }

  // Returns true if this UrlData has not yet had a length set, or if |length|
  // matches the existing length.
  bool HasCompatibleLength(int64_t length) const;

  // Returns the number of blocks cached for this resource.
  size_t CachedSize();

  // Returns true if this resource is fully cached in memory.
  bool FullyCached();

  // Returns our url_index.
  base::WeakPtr<UrlIndex> url_index() const { return url_index_; }

  // This must be called after the response arrives.
  bool is_cors_cross_origin() const { return is_cors_cross_origin_; }

  // True if loading this UrlData traversed a cross-origin redirect that must be
  // replayed on follow-up range requests (e.g. a cross-origin bounce-back).
  bool has_cross_origin_redirect() const { return has_cross_origin_redirect_; }

  // True if this UrlData received an opaque response that was fetched via a
  // service worker.
  bool HasOpaqueServiceWorkerResponse() const {
    return has_opaque_service_worker_response_;
  }

  // Notifies the url index that this is currently used.
  // The url <-> URLData mapping will be eventually be invalidated if
  // this is not called regularly.
  void Use();

  // Returns true if this UrlData has not yet had a data origin set, or if
  // |origin|, |is_cors_cross_origin|, |has_cross_origin_redirect|, and
  // |was_fetched_via_service_worker| are compatible with the existing state.
  bool CanAcceptDataFrom(const KURL& origin,
                         bool is_cors_cross_origin,
                         bool has_cross_origin_redirect = false,
                         bool was_fetched_via_service_worker = false) const;

  // Sets the data origin, CORS cross-origin state, cross-origin redirect
  // state, and service worker provenance. Must only be called when
  // CanAcceptDataFrom() is true.
  void SetDataOrigin(const KURL& origin,
                     bool is_cors_cross_origin,
                     bool has_cross_origin_redirect = false,
                     bool was_fetched_via_service_worker = false);

  // Setters.
  void set_length(int64_t length);
  void set_cacheable(bool cacheable);
  void set_valid_until(base::Time valid_until);
  void set_range_supported();
  void set_last_modified(base::Time last_modified);
  void set_etag(const std::string& etag);
  void set_has_access_control();
  void set_passed_timing_allow_origin_check(bool);
  void set_mime_type(std::string mime_type);

  // A redirect has occurred (or we've found a better UrlData for the same
  // resource).
  void RedirectTo(const scoped_refptr<UrlData>& to);

  // Fail, tell all clients that a failure has occurred. Permanently marks this
  // UrlData as invalid for reuse via UrlIndex, and immediately evicts it from
  // UrlIndex if it has no cached blocks.
  void Fail();

  // Marks this data as CORS cross-origin if no data origin has been established
  // yet, then calls Fail(). Used when a request fails due to cross-origin
  // security restrictions (e.g., ORB, CORP, or CORS violations) so initial
  // cross-origin blocks are marked cross-origin without mutating the taint
  // state of an already-established UrlData.
  void CrossOriginFail();

  // Callback for receiving notifications when a redirect occurs.
  using RedirectCB = base::OnceCallback<void(const scoped_refptr<UrlData>&)>;

  // Register a callback to be called when a redirect occurs.
  // Callbacks are cleared when a redirect occurs, so clients must call
  // OnRedirect again if they wish to continue receiving callbacks.
  void OnRedirect(RedirectCB cb);

  // Returns true it is valid to keep using this to access cached data.
  // A single media player instance may choose to ignore this for resources
  // that have already been opened.
  bool Valid();

  // True once Fail() has been called on this UrlData or its owning UrlIndex
  // has been destroyed.
  bool HasFailed() const;

  // Virtual so we can override it for testing.
  virtual ResourceMultiBuffer* multibuffer();

  void AddBytesRead(int64_t b) { bytes_read_from_cache_ += b; }
  int64_t BytesReadFromCache() const { return bytes_read_from_cache_; }

 protected:
  UrlData(const KURL& url,
          CorsMode cors_mode,
          CacheMode cache_lookup_mode,
          scoped_refptr<const SecurityOrigin> security_origin,
          media::DataSource::EncodingMode encoding_mode,
          ServiceWorkerMode service_worker_mode,
          const KURL& original_request_url,
          base::WeakPtr<UrlIndex> url_index,
          scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~UrlData();

 private:
  friend class ResourceMultiBuffer;
  friend class RefCounted<UrlData>;
  friend class UrlIndex;

  void OnEmpty();
  bool MergeFrom(const scoped_refptr<UrlData>& other);
  void set_is_cors_cross_origin(bool is_cors_cross_origin);

  // Url we represent, note that there may be multiple UrlData for
  // the same url.
  const KURL url_;

  // Non-empty only for unindexed bounce-back redirects, where `url_` holds the
  // post-redirect URL while follow-up range requests replay from
  // `original_request_url_`.
  const KURL original_request_url_;

  // Requesting document's security origin.
  const scoped_refptr<const SecurityOrigin> security_origin_;

  // Full response URL that established the resource's origin (empty for
  // constructed `new Response()` service worker responses). May differ from
  // `url_` due to redirects, HTTPS upgrades, or service worker responses.
  std::optional<KURL> data_origin_;

  // Cross-origin access mode.
  const CorsMode cors_mode_;
  bool has_access_control_;

  // Timing-allow-origin (unset until the first response is received; once any
  // response fails TAO, latched to false).
  std::optional<bool> passed_timing_allow_origin_check_;

  // Mime type category (stashed for UMA / metrics).
  std::string mime_type_;

  const base::WeakPtr<UrlIndex> url_index_;

  // Length of resource this url points to. (in bytes)
  int64_t length_;

  // Number of bytes read from this resource.
  int64_t bytes_read_from_cache_ = 0;

  // Does the server support ranges?
  bool range_supported_;

  // Set to false if we have reason to believe the chrome disk cache
  // will not cache this url.
  bool cacheable_;

  // While `cacheable_` determines whether this UrlData's underlying data should
  // be stored in the cache, `cache_lookup_mode_` determines whether this
  // UrlData should use existing underlying cached data.
  CacheMode cache_lookup_mode_;

  media::DataSource::EncodingMode encoding_mode_;

  // Controls whether range requests for this UrlData may be intercepted by a
  // Service Worker (`kAllow`) or must bypass Service Workers (`kSkip`, latched
  // once a redirect hop not synthesized by a Service Worker is followed).
  // Included in `UrlData::Key` so post-redirect loads do not share `UrlIndex`
  // entries with normal Service-Worker-eligible loads.
  ServiceWorkerMode service_worker_mode_;

  // https://html.spec.whatwg.org/#cors-cross-origin
  bool is_cors_cross_origin_ = false;

  // True if loading this UrlData traversed a cross-origin redirect that must be
  // replayed on follow-up range requests.
  bool has_cross_origin_redirect_ = false;

  // True if any response establishing or merged into this UrlData was an
  // opaque response fetched via a service worker.
  bool has_opaque_service_worker_response_ = false;

  // True once Fail() has been called on this UrlData.
  bool has_failed_ = false;

  // Last time some media time used this resource.
  // Note that we use base::Time rather than base::TimeTicks because
  // TimeTicks will stop advancing when a machine goes to sleep.
  // base::Time can go backwards, jump hours at a time and be generally
  // unpredictable, but it doesn't stop, which is preferable here.
  // (False negatives are better than false positivies.)
  base::Time last_used_;

  // Expiration time according to http headers.
  base::Time valid_until_;

  // Last modification time according to http headers.
  base::Time last_modified_;

  // Etag from HTTP reply.
  std::string etag_;

  ResourceMultiBuffer multibuffer_;
  Vector<RedirectCB> redirect_callbacks_;

  THREAD_CHECKER(thread_checker_);
};

template <>
struct HashTraits<UrlData::Key>
    : OneFieldHashTraits<UrlData::Key, &UrlData::Key::security_origin> {
  static constexpr bool kEmptyValueIsZero = false;
  static UrlData::Key EmptyValue() { return UrlData::Key(); }

  static uint32_t GetHash(const UrlData::Key& key) {
    uint32_t hash = key.url.IsNull() ? 0 : blink::GetHash(key.url);
    hash = HashInts(hash, blink::GetHash(key.security_origin));
    hash = HashInts(hash, blink::GetHash(key.cors_mode));
    hash = HashInts(hash, blink::GetHash(key.encoding_mode));
    return HashInts(hash, blink::GetHash(key.service_worker_mode));
  }

  static bool Equal(const UrlData::Key& a, const UrlData::Key& b) {
    return a.url == b.url &&
           HashTraits<scoped_refptr<const SecurityOrigin>>::Equal(
               a.security_origin, b.security_origin) &&
           a.cors_mode == b.cors_mode && a.encoding_mode == b.encoding_mode &&
           a.service_worker_mode == b.service_worker_mode;
  }
};

// The UrlIndex lets you look up UrlData instances by url.
class PLATFORM_EXPORT UrlIndex {
 public:
  UrlIndex(ResourceFetchContext* fetch_context,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UrlIndex(ResourceFetchContext* fetch_context,
           int block_shift,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~UrlIndex();

  // Look up an UrlData in the index and return it. If none is found,
  // create a new one. Note that newly created UrlData entries are NOT
  // added to the index, instead you must call TryInsert on them after
  // initializing relevant parameters, like whether it support
  // ranges and it's last modified time.
  // Because the returned UrlData has a raw reference to |this|, it must be
  // released before |this| is destroyed.
  scoped_refptr<UrlData> GetByUrl(
      const KURL& url,
      UrlData::CorsMode cors_mode,
      UrlData::CacheMode cache_mode,
      scoped_refptr<const SecurityOrigin> security_origin,
      media::DataSource::EncodingMode encoding_mode =
          media::DataSource::EncodingMode::kIdentity,
      UrlData::ServiceWorkerMode service_worker_mode =
          UrlData::ServiceWorkerMode::kAllow);

  // Creates an unindexed `UrlData` -- one that is never stored in or returned
  // by `UrlIndex` (`TryInsert()`, `MergeFrom()`, and `RemoveUrlData()` ignore
  // it), keeping its cached blocks and tainting private to its caller. Its
  // post-redirect URL is `url` (`url()`), while follow-up range requests replay
  // from `original_request_url` (`request_url()`). Used when direct follow-up
  // range requests to `url` would lose redirect-chain state (such as
  // bounce-back redirects or cross-origin redirects under universal/extension
  // access), preserving `net::URLRequest::url_chain()` without colliding with
  // or tainting direct `UrlIndex` entries for `url`.
  scoped_refptr<UrlData> NewUnindexedUrlData(
      const KURL& url,
      UrlData::CorsMode cors_mode,
      UrlData::CacheMode cache_mode,
      scoped_refptr<const SecurityOrigin> security_origin,
      media::DataSource::EncodingMode encoding_mode,
      UrlData::ServiceWorkerMode service_worker_mode,
      const KURL& original_request_url);

  // Add the given UrlData to the index if possible. If a better UrlData
  // is already present in the index, return it instead. (If not, we just
  // return the given UrlData.) Please make sure to initialize all the data
  // that can be gathered from HTTP headers in |url_data| before calling this.
  // In particular, the following fields are important:
  //   o range_supported: Entries which do not support ranges cannot be
  //     shared and are not added to the index.
  //   o valid_until, last_used: Entries have to be valid to be inserted
  //     into the index, this means that they have to have been recently
  //     used or have an Expires: header that says when they stop being valid.
  //   o last_modified: Expired cache entries can be re-used if last_modified
  //     matches.
  // Because the returned UrlData has a raw reference to |this|, it must be
  // released before |this| is destroyed.
  // TODO(hubbe): Add etag support.
  scoped_refptr<UrlData> TryInsert(const scoped_refptr<UrlData>& url_data);

  ResourceFetchContext* fetch_context() const { return fetch_context_; }
  int block_shift() const { return block_shift_; }

  // Protected rather than private for testing.
 protected:
  friend class UrlData;
  friend class ResourceMultiBuffer;
  void RemoveUrlData(const scoped_refptr<UrlData>& url_data);

  // Virtual so we can override it in tests.
  virtual scoped_refptr<UrlData> NewUrlData(
      const KURL& url,
      UrlData::CorsMode cors_mode,
      UrlData::CacheMode cache_lookup_mode,
      scoped_refptr<const SecurityOrigin> security_origin,
      media::DataSource::EncodingMode encoding_mode,
      UrlData::ServiceWorkerMode service_worker_mode,
      const KURL& original_request_url);

  raw_ptr<ResourceFetchContext> fetch_context_;
  using UrlDataMap = HashMap<UrlData::Key, scoped_refptr<UrlData>>;
  UrlDataMap indexed_data_;
  scoped_refptr<MultiBuffer::GlobalLRU> lru_;

  // log2 of block size in multibuffer cache. Defaults to kBlockSizeShift.
  // Currently only changed for testing purposes.
  const int block_shift_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<UrlIndex> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_URL_INDEX_H_
