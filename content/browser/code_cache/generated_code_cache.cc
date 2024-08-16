// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/code_cache/generated_code_cache.h"

#include <iostream>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "crypto/sha2.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/url_util.h"
#include "net/http/http_cache.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "url/gurl.h"

using storage::BigIOBuffer;

namespace content {

namespace {

constexpr char kPrefix[] = "_key";
constexpr char kSeparator[] = " \n";

// We always expect to receive valid URLs that can be used as keys to the code
// cache. The relevant checks (for ex: resource_url is valid, origin_lock is
// not opque etc.,) must be done prior to requesting the code cache.
//
// This function doesn't enforce anything in the production code. It is here
// to make the assumptions explicit and to catch any errors when DCHECKs are
// enabled.
void CheckValidKeys(const GURL& resource_url,
                    const GURL& origin_lock,
                    GeneratedCodeCache::CodeCacheType cache_type) {
  // If the resource url is invalid don't cache the code.
  DCHECK(resource_url.is_valid());
  bool resource_url_is_chrome_or_chrome_untrusted =
      resource_url.SchemeIs(content::kChromeUIScheme) ||
      resource_url.SchemeIs(content::kChromeUIUntrustedScheme);
  DCHECK(resource_url.SchemeIsHTTPOrHTTPS() ||
         resource_url_is_chrome_or_chrome_untrusted ||
         blink::CommonSchemeRegistry::IsExtensionScheme(resource_url.scheme()));

  // |origin_lock| should be either empty or should have
  // Http/Https/chrome/chrome-untrusted schemes and it should not be a URL with
  // opaque origin. Empty origin_locks are allowed when the renderer is not
  // locked to an origin.
  bool origin_lock_is_chrome_or_chrome_untrusted =
      origin_lock.SchemeIs(content::kChromeUIScheme) ||
      origin_lock.SchemeIs(content::kChromeUIUntrustedScheme);
  DCHECK(
      origin_lock.is_empty() ||
      ((origin_lock.SchemeIsHTTPOrHTTPS() ||
        origin_lock_is_chrome_or_chrome_untrusted ||
        blink::CommonSchemeRegistry::IsExtensionScheme(origin_lock.scheme())) &&
       !url::Origin::Create(origin_lock).opaque()));

  // The chrome and chrome-untrusted schemes are only used with the WebUI
  // code cache type.
  DCHECK_EQ(origin_lock_is_chrome_or_chrome_untrusted,
            cache_type == GeneratedCodeCache::kWebUIJavaScript);
  DCHECK_EQ(resource_url_is_chrome_or_chrome_untrusted,
            cache_type == GeneratedCodeCache::kWebUIJavaScript);
}

// Generates the cache key for the given |resource_url|, |origin_lock| and
// |nik|.
//   |resource_url| is the url corresponding to the requested resource.
//   |origin_lock| is the origin that the renderer which requested this
//   resource is locked to.
//   |nik| is the network isolation key that consists of top-level-site that
//   initiated the request.
// For example, if SitePerProcess is enabled and http://script.com/script1.js is
// requested by http://example.com, then http://script.com/script.js is the
// resource_url and http://example.com is the origin_lock.
//
// This returns the key by concatenating the serialized url, origin lock and nik
// with a separator in between. |origin_lock| could be empty when renderer is
// not locked to an origin (ex: SitePerProcess is disabled) and it is safe to
// use only |resource_url| as the key in such cases.
// TODO(wjmaclean): Either convert this to use a SiteInfo object, or convert it
// to something not based on URLs.
std::string GetCacheKey(const GURL& resource_url,
                        const GURL& origin_lock,
                        const net::NetworkIsolationKey& nik,
                        GeneratedCodeCache::CodeCacheType cache_type) {
  CheckValidKeys(resource_url, origin_lock, cache_type);

  // Add a prefix _ so it can't be parsed as a valid URL.
  std::string key(kPrefix);
  // Remove reference, username and password sections of the URL.
  key.append(net::SimplifyUrlForRequest(resource_url).spec());
  // Add a separator between URL and origin to avoid any possibility of
  // attacks by crafting the URL. URLs do not contain any control ASCII
  // characters, and also space is encoded. So use ' \n' as a seperator.
  key.append(kSeparator);

  if (origin_lock.is_valid())
    key.append(net::SimplifyUrlForRequest(origin_lock).spec());

  if (net::HttpCache::IsSplitCacheEnabled() &&
      base::FeatureList::IsEnabled(
          net::features::kSplitCodeCacheByNetworkIsolationKey)) {
    // TODO(crbug.com/40232395):  Transient NIKs return nullopt when
    // their ToCacheKeyString() method is invoked, as they generally shouldn't
    // be written to disk. This code is currently reached for transient NIKs,
    // which needs to be fixed.
    if (!nik.IsTransient()) {
      key.append(kSeparator);
      key.append(*nik.ToCacheKeyString());
    }
  }
  return key;
}

constexpr size_t kResponseTimeSizeInBytes = sizeof(int64_t);
constexpr size_t kDataSizeInBytes = sizeof(uint32_t);
constexpr size_t kHeaderSizeInBytes =
    kResponseTimeSizeInBytes + kDataSizeInBytes;
// The SHA-256 checksum is used as the key for the de-duplicated code data. We
// must convert the checksum to a string key in a way that is guaranteed not to
// match a key generated by |GetCacheKey|. A simple way to do this is to convert
// it to a hex number string, which is twice as long as the checksum.
constexpr size_t kSHAKeySizeInBytes = 2 * crypto::kSHA256Length;

// This is the threshold for storing the header and cached code in stream 0,
// which is read into memory on opening an entry. JavaScript code caching stores
// time stamps with no data, or timestamps with just a tag, and we observe many
// 8 and 16 byte reads and writes. Make the threshold larger to speed up small
// code entries too.
constexpr size_t kInlineDataLimit = 4096;
// This is the maximum size for code that will be stored under the key generated
// by |GetCacheKey|. Each origin will get its own copy of the generated code for
// a given resource. Code that is larger than this limit will be stored under a
// key derived from the code checksum, and each origin using a given resource
// gets its own small entry under the key generated by |GetCacheKey| that holds
// the hash, enabling a two stage lookup. This limit was determined empirically
// by a Finch experiment.
constexpr size_t kDedicatedDataLimit = 16384;

void WriteCommonDataHeader(net::IOBufferWithSize* buffer,
                           const base::Time& response_time,
                           uint32_t data_size) {
  auto header = buffer->span().first<kHeaderSizeInBytes>();
  auto [header_time, header_size] = header.split_at<kResponseTimeSizeInBytes>();
  header_time.copy_from(base::I64ToLittleEndian(
      response_time.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  header_size.copy_from(base::U32ToLittleEndian(data_size));
}

void ReadCommonDataHeader(net::IOBufferWithSize* buffer,
                          base::Time* response_time,
                          uint32_t* data_size) {
  auto header = buffer->span().first<kHeaderSizeInBytes>();
  auto [header_time, header_size] = header.split_at<kResponseTimeSizeInBytes>();
  int64_t raw_response_time = base::I64FromLittleEndian(header_time);
  *response_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(raw_response_time));
  *data_size = base::U32FromLittleEndian(header_size);
}

static_assert(mojo_base::BigBuffer::kMaxInlineBytes <=
                  std::numeric_limits<int>::max(),
              "Buffer size calculations may overflow int");

net::CacheType CodeCacheTypeToNetCacheType(
    GeneratedCodeCache::CodeCacheType type) {
  switch (type) {
    case GeneratedCodeCache::CodeCacheType::kJavaScript:
      return net::GENERATED_BYTE_CODE_CACHE;
    case GeneratedCodeCache::CodeCacheType::kWebAssembly:
      return net::GENERATED_NATIVE_CODE_CACHE;
    case GeneratedCodeCache::CodeCacheType::kWebUIJavaScript:
      return net::GENERATED_WEBUI_BYTE_CODE_CACHE;
  }
  NOTREACHED_IN_MIGRATION();
}

void CollectStatisticsForEmbedderWebUIPages(
    const GURL& resource_url,
    const GURL& origin_lock,
    GeneratedCodeCache::CacheEntryStatus entry_status) {
  const content::ContentBrowserClient* browser_client =
      GetContentClient()->browser();
  CHECK(browser_client);
  const std::string resource_hostname =
      browser_client->GetWebUIHostnameForCodeCacheMetrics(resource_url);
  const std::string origin_hostname =
      browser_client->GetWebUIHostnameForCodeCacheMetrics(origin_lock);

  if (!resource_hostname.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"SiteIsolatedCodeCache.JS.WebUI.",
                      std::move(resource_hostname), ".Resource.Behaviour"}),
        entry_status);
  }

  if (!origin_hostname.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"SiteIsolatedCodeCache.JS.WebUI.",
                      std::move(origin_hostname), ".Origin.Behaviour"}),
        entry_status);
  }
}

}  // namespace

bool GeneratedCodeCache::IsValidHeader(
    scoped_refptr<net::IOBufferWithSize> small_buffer) const {
  size_t buffer_size = small_buffer->size();
  if (buffer_size < kHeaderSizeInBytes) {
    return false;
  }
  base::Time response_time;
  uint32_t data_size = 0;
  ReadCommonDataHeader(small_buffer.get(), &response_time, &data_size);
  if (data_size <= kInlineDataLimit) {
    return buffer_size == kHeaderSizeInBytes + data_size;
  }
  if (!ShouldDeduplicateEntry(data_size)) {
    return buffer_size == kHeaderSizeInBytes;
  }
  return buffer_size == kHeaderSizeInBytes + kSHAKeySizeInBytes;
}

void GeneratedCodeCache::ReportPeriodicalHistograms() {
  DCHECK_EQ(cache_type_, CodeCacheType::kJavaScript);
  base::UmaHistogramCustomCounts(
      "SiteIsolatedCodeCache.JS.PotentialMemoryBackedCodeCacheSize2",
      lru_cache_.GetSize(),
      /*min=*/0,
      /*exclusive_max=*/kLruCacheCapacity,
      /*buckets=*/50);
}

std::string GeneratedCodeCache::GetResourceURLFromKey(const std::string& key) {
  constexpr size_t kPrefixStringLen = std::size(kPrefix) - 1;
  // |key| may not have a prefix and separator (e.g. for deduplicated entries).
  // In that case, return an empty string.
  const size_t separator_index = key.find(kSeparator);
  if (key.length() < kPrefixStringLen || separator_index == std::string::npos) {
    return std::string();
  }

  std::string resource_url =
      key.substr(kPrefixStringLen, separator_index - kPrefixStringLen);
  return resource_url;
}

void GeneratedCodeCache::CollectStatistics(
    const GURL& resource_url,
    const GURL& origin_lock,
    GeneratedCodeCache::CacheEntryStatus status) {
  switch (cache_type_) {
    case GeneratedCodeCache::CodeCacheType::kJavaScript:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.JS.Behaviour", status);
      break;
    case GeneratedCodeCache::CodeCacheType::kWebUIJavaScript:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.JS.Behaviour", status);
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.JS.WebUI.Behaviour",
                                status);
      CollectStatisticsForEmbedderWebUIPages(resource_url, origin_lock, status);
      break;
    case GeneratedCodeCache::CodeCacheType::kWebAssembly:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.WASM.Behaviour", status);
      break;
  }
}

// Stores the information about a pending request while disk backend is
// being initialized or another request for the same key is live.
class GeneratedCodeCache::PendingOperation {
 public:
  PendingOperation(Operation op,
                   const GURL& resource_url,
                   const GURL& origin_lock,
                   const std::string& key,
                   scoped_refptr<net::IOBufferWithSize> small_buffer,
                   scoped_refptr<BigIOBuffer> large_buffer)
      : op_(op),
        resource_url_(resource_url),
        origin_lock_(origin_lock),
        key_(key),
        small_buffer_(small_buffer),
        large_buffer_(large_buffer) {
    DCHECK(Operation::kWrite == op_ || Operation::kWriteWithSHAKey == op_);
  }

  PendingOperation(Operation op,
                   const GURL& resource_url,
                   const GURL& origin_lock,
                   const std::string& key,
                   ReadDataCallback read_callback)
      : op_(op),
        resource_url_(resource_url),
        origin_lock_(origin_lock),
        key_(key),
        read_callback_(std::move(read_callback)) {
    DCHECK_EQ(Operation::kFetch, op_);
  }

  PendingOperation(Operation op,
                   const GURL& resource_url,
                   const GURL& origin_lock,
                   const std::string& key,
                   const base::Time& response_time,
                   const base::TimeTicks start_time,
                   scoped_refptr<net::IOBufferWithSize> small_buffer,
                   scoped_refptr<BigIOBuffer> large_buffer,
                   ReadDataCallback read_callback)
      : op_(op),
        resource_url_(resource_url),
        origin_lock_(origin_lock),
        key_(key),
        response_time_(response_time),
        start_time_(start_time),
        small_buffer_(small_buffer),
        large_buffer_(large_buffer),
        read_callback_(std::move(read_callback)) {
    DCHECK_EQ(Operation::kFetchWithSHAKey, op_);
  }

  PendingOperation(Operation op,
                   const GURL& resource_url,
                   const GURL& origin_lock,
                   const std::string& key)
      : op_(op),
        resource_url_(resource_url),
        origin_lock_(origin_lock),
        key_(key) {
    DCHECK_EQ(Operation::kDelete, op_);
  }

  PendingOperation(Operation op, GetBackendCallback backend_callback)
      : op_(op), backend_callback_(std::move(backend_callback)) {
    DCHECK_EQ(Operation::kGetBackend, op_);
  }

  ~PendingOperation();

  Operation operation() const { return op_; }
  const std::string& key() const { return key_; }
  const GURL& resource_url() const { return resource_url_; }
  const GURL& origin_lock() const { return origin_lock_; }
  scoped_refptr<net::IOBufferWithSize> small_buffer() { return small_buffer_; }
  scoped_refptr<BigIOBuffer> large_buffer() { return large_buffer_; }
  ReadDataCallback TakeReadCallback() { return std::move(read_callback_); }
  void RunReadCallback(GeneratedCodeCache* code_cache,
                       base::Time response_time,
                       mojo_base::BigBuffer data) {
    if (code_cache->cache_type_ == CodeCacheType::kJavaScript) {
      const bool code_cache_hit = data.size() > 0;
      const bool in_memory_code_cache_hit = code_cache->lru_cache_.Has(key_);
      if (code_cache_hit && !in_memory_code_cache_hit) {
        code_cache->lru_cache_.Put(key_, response_time, base::make_span(data));
      }
      if (!base::FeatureList::IsEnabled(features::kInMemoryCodeCache)) {
        if (code_cache_hit && in_memory_code_cache_hit) {
          base::UmaHistogramTimes(
              "SiteIsolatedCodeCache.JS.MemoryBackedCodeCachePotentialImpact",
              base::TimeTicks::Now() - start_time_);
        }
        base::UmaHistogramBoolean("SiteIsolatedCodeCache.JS.Hit",
                                  code_cache_hit);
        base::UmaHistogramBoolean(
            "SiteIsolatedCodeCache.JS.PotentialMemoryBackedCodeCacheHit",
            in_memory_code_cache_hit);
      }
    }
    std::move(read_callback_).Run(response_time, std::move(data));
  }
  GetBackendCallback TakeBackendCallback() {
    return std::move(backend_callback_);
  }

  // These are called by Fetch operations to hold the buffers we create once the
  // entry is opened.
  void set_small_buffer(scoped_refptr<net::IOBufferWithSize> small_buffer) {
    DCHECK_EQ(Operation::kFetch, op_);
    small_buffer_ = small_buffer;
  }
  void set_large_buffer(scoped_refptr<BigIOBuffer> large_buffer) {
    DCHECK_EQ(Operation::kFetch, op_);
    large_buffer_ = large_buffer;
  }

  // This returns the site-specific response time for merged code entries.
  const base::Time& response_time() const {
    DCHECK_EQ(Operation::kFetchWithSHAKey, op_);
    return response_time_;
  }

  base::TimeTicks start_time() const { return start_time_; }

  // These are called by write and fetch operations to track buffer completions
  // and signal when the operation has finished, and whether it was successful.
  bool succeeded() const { return succeeded_; }

  bool AddBufferCompletion(bool succeeded) {
    DCHECK(op_ == Operation::kWrite || op_ == Operation::kWriteWithSHAKey ||
           op_ == Operation::kFetch || op_ == Operation::kFetchWithSHAKey);
    if (!succeeded)
      succeeded_ = false;
    DCHECK_GT(2, completions_);
    completions_++;
    return completions_ == 2;
  }

 private:
  const Operation op_;
  const GURL resource_url_;
  const GURL origin_lock_;
  const std::string key_;
  const base::Time response_time_;
  const base::TimeTicks start_time_ = base::TimeTicks::Now();
  scoped_refptr<net::IOBufferWithSize> small_buffer_;
  scoped_refptr<BigIOBuffer> large_buffer_;
  ReadDataCallback read_callback_;
  GetBackendCallback backend_callback_;
  int completions_ = 0;
  bool succeeded_ = true;
};

GeneratedCodeCache::PendingOperation::~PendingOperation() = default;

GeneratedCodeCache::GeneratedCodeCache(const base::FilePath& path,
                                       int max_size_bytes,
                                       CodeCacheType cache_type)
    : backend_state_(kInitializing),
      path_(path),
      max_size_bytes_(max_size_bytes),
      cache_type_(cache_type),
      lru_cache_(max_size_bytes == 0
                     ? kLruCacheCapacity
                     : std::min<int64_t>(kLruCacheCapacity, max_size_bytes)) {
  CreateBackend();
  if (cache_type == CodeCacheType::kJavaScript) {
    histograms_timer_.Start(
        FROM_HERE, base::Minutes(5),
        base::BindRepeating(&GeneratedCodeCache::ReportPeriodicalHistograms,
                            base::Unretained(this)));
  }
}

GeneratedCodeCache::~GeneratedCodeCache() = default;

void GeneratedCodeCache::GetBackend(GetBackendCallback callback) {
  switch (backend_state_) {
    case kFailed:
      std::move(callback).Run(nullptr);
      return;
    case kInitialized:
      std::move(callback).Run(backend_.get());
      return;
    case kInitializing:
      pending_ops_.emplace(std::make_unique<PendingOperation>(
          Operation::kGetBackend, std::move(callback)));
      return;
  }
}

void GeneratedCodeCache::WriteEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    const net::NetworkIsolationKey& nik,
                                    const base::Time& response_time,
                                    mojo_base::BigBuffer data) {
  if (backend_state_ == kFailed) {
    // Silently fail the request.
    CollectStatistics(url, origin_lock, CacheEntryStatus::kError);
    return;
  }

  // Reject buffers that are large enough to cause overflow problems.
  if (data.size() >= std::numeric_limits<int32_t>::max())
    return;

  const std::string key = GetCacheKey(url, origin_lock, nik, cache_type_);
  if (cache_type_ == CodeCacheType::kJavaScript) {
    lru_cache_.Put(key, response_time, base::make_span(data));
  }

  scoped_refptr<net::IOBufferWithSize> small_buffer;
  scoped_refptr<BigIOBuffer> large_buffer;
  const uint32_t data_size = static_cast<uint32_t>(data.size());
  // We have three different cache entry layouts, depending on data size.
  if (data_size <= kInlineDataLimit) {
    // 1. Inline
    // [stream0] response time, size, data
    // [stream1] <empty>
    small_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
        kHeaderSizeInBytes + data.size());
    // Copy |data| into the small buffer.
    small_buffer->span().subspan(kHeaderSizeInBytes).copy_from(data);
    // Write 0 bytes and truncate stream 1 to clear any stale data.
    large_buffer = base::MakeRefCounted<BigIOBuffer>(mojo_base::BigBuffer());
  } else if (!ShouldDeduplicateEntry(data_size)) {
    // 2. Dedicated
    // [stream0] response time, size
    // [stream1] data
    small_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(kHeaderSizeInBytes);
    large_buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  } else {
    // 3. Indirect
    // [stream0] response time, size, checksum
    // [stream1] <empty>
    // [stream0 (checksum key entry)] <empty>
    // [stream1 (checksum key entry)] data

    // Make a copy of the data before hashing. A compromised renderer could
    // change shared memory before we can compute the hash and write the data.
    // TODO(crbug.com/40151989) Eliminate this copy when the shared memory can't
    // be written by the sender.
    mojo_base::BigBuffer copy({data.data(), data.size()});
    if (copy.size() != data.size())
      return;
    data = mojo_base::BigBuffer();  // Release the old buffer.
    uint8_t result[crypto::kSHA256Length];
    crypto::SHA256HashString(
        std::string_view(reinterpret_cast<char*>(copy.data()), copy.size()),
        result, std::size(result));
    std::string checksum_key = base::HexEncode(result);
    DCHECK_EQ(kSHAKeySizeInBytes, checksum_key.length());
    small_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
        kHeaderSizeInBytes + checksum_key.length());
    // Copy |checksum_key| into the small buffer.
    small_buffer->span()
        .subspan(kHeaderSizeInBytes)
        .copy_from(base::as_byte_span(checksum_key));
    // Write 0 bytes and truncate stream 1 to clear any stale data.
    large_buffer = base::MakeRefCounted<BigIOBuffer>(mojo_base::BigBuffer());

    // Issue another write operation for the code, with the checksum as the key
    // and nothing in the header.
    auto small_buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(0);
    auto large_buffer2 = base::MakeRefCounted<BigIOBuffer>(std::move(copy));
    auto op2 = std::make_unique<PendingOperation>(
        Operation::kWriteWithSHAKey, url, origin_lock, checksum_key,
        small_buffer2, large_buffer2);
    EnqueueOperation(std::move(op2));
  }
  WriteCommonDataHeader(small_buffer.get(), response_time, data_size);

  // Create the write operation.
  auto op = std::make_unique<PendingOperation>(
      Operation::kWrite, url, origin_lock, key, small_buffer, large_buffer);
  EnqueueOperation(std::move(op));
}

void GeneratedCodeCache::FetchEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    const net::NetworkIsolationKey& nik,
                                    ReadDataCallback read_data_callback) {
  if (backend_state_ == kFailed) {
    CollectStatistics(url, origin_lock, CacheEntryStatus::kError);
    // Fail the request.
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
    return;
  }

  std::string key = GetCacheKey(url, origin_lock, nik, cache_type_);
  auto op = std::make_unique<PendingOperation>(
      Operation::kFetch, url, origin_lock, key, std::move(read_data_callback));
  EnqueueOperation(std::move(op));
}

void GeneratedCodeCache::DeleteEntry(const GURL& url,
                                     const GURL& origin_lock,
                                     const net::NetworkIsolationKey& nik) {
  if (backend_state_ == kFailed) {
    // Silently fail.
    CollectStatistics(url, origin_lock, CacheEntryStatus::kError);
    return;
  }

  std::string key = GetCacheKey(url, origin_lock, nik, cache_type_);
  auto op = std::make_unique<PendingOperation>(Operation::kDelete, url,
                                               origin_lock, key);
  EnqueueOperation(std::move(op));

  lru_cache_.Delete(key);
}

void GeneratedCodeCache::CreateBackend() {
  // If the initialization of the existing cache fails, this call would delete
  // all the contents and recreates a new one.
  disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
      CodeCacheTypeToNetCacheType(cache_type_), net::CACHE_BACKEND_SIMPLE,
      /*file_operations=*/nullptr, path_, max_size_bytes_,
      disk_cache::ResetHandling::kResetOnError, /*net_log=*/nullptr,
      base::BindOnce(&GeneratedCodeCache::DidCreateBackend,
                     weak_ptr_factory_.GetWeakPtr()));
  if (result.net_error != net::ERR_IO_PENDING) {
    DidCreateBackend(std::move(result));
  }
}

void GeneratedCodeCache::DidCreateBackend(disk_cache::BackendResult result) {
  if (result.net_error != net::OK) {
    backend_state_ = kFailed;
  } else {
    backend_ = std::move(result.backend);
    backend_state_ = kInitialized;
  }
  IssuePendingOperations();
}

void GeneratedCodeCache::EnqueueOperation(
    std::unique_ptr<PendingOperation> op) {
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.emplace(std::move(op));
    return;
  }

  EnqueueOperationAndIssueIfNext(std::move(op));
}

void GeneratedCodeCache::IssuePendingOperations() {
  // Issue any operations that were received while creating the backend.
  while (!pending_ops_.empty()) {
    // Take ownership of the next PendingOperation here. |op| will either be
    // moved onto a queue in active_entries_map_ or issued and completed in
    // |DoPendingGetBackend|.
    std::unique_ptr<PendingOperation> op = std::move(pending_ops_.front());
    pending_ops_.pop();
    // Properly enqueue/dequeue ops for Write, Fetch, and Delete.
    if (op->operation() != Operation::kGetBackend) {
      EnqueueOperationAndIssueIfNext(std::move(op));
    } else {
      // There is no queue for get backend operations. Issue them immediately.
      IssueOperation(op.get());
    }
  }
}

void GeneratedCodeCache::IssueOperation(PendingOperation* op) {
  switch (op->operation()) {
    case kFetch:
    case kFetchWithSHAKey:
      FetchEntryImpl(op);
      break;
    case kWrite:
    case kWriteWithSHAKey:
      WriteEntryImpl(op);
      break;
    case kDelete:
      DeleteEntryImpl(op);
      break;
    case kGetBackend:
      DoPendingGetBackend(op);
      break;
  }
}

void GeneratedCodeCache::WriteEntryImpl(PendingOperation* op) {
  DCHECK(Operation::kWrite == op->operation() ||
         Operation::kWriteWithSHAKey == op->operation());
  if (backend_state_ != kInitialized) {
    // Silently fail the request.
    CloseOperationAndIssueNext(op);
    return;
  }

  disk_cache::EntryResult result = backend_->OpenOrCreateEntry(
      op->key(), net::LOW,
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForWrite,
                     weak_ptr_factory_.GetWeakPtr(), op));

  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForWrite(op, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForWrite(
    PendingOperation* op,
    disk_cache::EntryResult entry_result) {
  DCHECK(Operation::kWrite == op->operation() ||
         Operation::kWriteWithSHAKey == op->operation());
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(op->resource_url(), op->origin_lock(),
                      CacheEntryStatus::kError);
    CloseOperationAndIssueNext(op);
    return;
  }

  if (entry_result.opened()) {
    CollectStatistics(op->resource_url(), op->origin_lock(),
                      CacheEntryStatus::kUpdate);
  } else {
    CollectStatistics(op->resource_url(), op->origin_lock(),
                      CacheEntryStatus::kCreate);
  }

  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());
  // There should be a valid entry if the open was successful.
  DCHECK(entry);

  // For merged entries, don't write if the entry already exists.
  if (op->operation() == Operation::kWriteWithSHAKey) {
    int small_size = entry->GetDataSize(kSmallDataStream);
    int large_size = entry->GetDataSize(kLargeDataStream);
    if (small_size == 0 && large_size == op->large_buffer()->size()) {
      // Skip overwriting with identical data.
      CloseOperationAndIssueNext(op);
      return;
    }
    // Otherwise, there shouldn't be any data for this entry yet.
    DCHECK_EQ(0, small_size);
    DCHECK_EQ(0, large_size);
  }

  // Write the small data first, truncating.
  auto small_buffer = op->small_buffer();
  int result = entry->WriteData(
      kSmallDataStream, 0, small_buffer.get(), small_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::WriteSmallBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op),
      true);

  if (result != net::ERR_IO_PENDING) {
    WriteSmallBufferComplete(op, result);
  }

  // Write the large data, truncating.
  auto large_buffer = op->large_buffer();
  result = entry->WriteData(
      kLargeDataStream, 0, large_buffer.get(), large_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::WriteLargeBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op),
      true);

  if (result != net::ERR_IO_PENDING) {
    WriteLargeBufferComplete(op, result);
  }
}

void GeneratedCodeCache::WriteSmallBufferComplete(PendingOperation* op,
                                                  int rv) {
  DCHECK(Operation::kWrite == op->operation() ||
         Operation::kWriteWithSHAKey == op->operation());
  if (op->AddBufferCompletion(rv == op->small_buffer()->size())) {
    WriteComplete(op);
  }
}

void GeneratedCodeCache::WriteLargeBufferComplete(PendingOperation* op,
                                                  int rv) {
  DCHECK(Operation::kWrite == op->operation() ||
         Operation::kWriteWithSHAKey == op->operation());
  if (op->AddBufferCompletion(rv == op->large_buffer()->size())) {
    WriteComplete(op);
  }
}

void GeneratedCodeCache::WriteComplete(PendingOperation* op) {
  DCHECK(Operation::kWrite == op->operation() ||
         Operation::kWriteWithSHAKey == op->operation());
  if (!op->succeeded()) {
    // The write failed; record the failure and doom the entry here.
    CollectStatistics(op->resource_url(), op->origin_lock(),
                      CacheEntryStatus::kWriteFailed);
    DoomEntry(op);
  }
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::FetchEntryImpl(PendingOperation* op) {
  DCHECK(Operation::kFetch == op->operation() ||
         Operation::kFetchWithSHAKey == op->operation());
  if (base::FeatureList::IsEnabled(features::kInMemoryCodeCache)) {
    if (auto result = lru_cache_.Get(op->key())) {
      op->RunReadCallback(this, result->response_time, std::move(result->data));
      CloseOperationAndIssueNext(op);
      return;
    }
  }

  if (backend_state_ != kInitialized) {
    op->RunReadCallback(this, base::Time(), mojo_base::BigBuffer());
    CloseOperationAndIssueNext(op);
    return;
  }

  // This is a part of loading cycle and hence should run with a high priority.
  disk_cache::EntryResult result = backend_->OpenEntry(
      op->key(), net::HIGHEST,
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForRead,
                     weak_ptr_factory_.GetWeakPtr(), op));
  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForRead(op, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForRead(
    PendingOperation* op,
    disk_cache::EntryResult entry_result) {
  DCHECK(Operation::kFetch == op->operation() ||
         Operation::kFetchWithSHAKey == op->operation());
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(op->resource_url(), op->origin_lock(),
                      CacheEntryStatus::kMiss);
    op->RunReadCallback(this, base::Time(), mojo_base::BigBuffer());
    CloseOperationAndIssueNext(op);
    return;
  }

  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());
  // There should be a valid entry if the open was successful.
  DCHECK(entry);

  int small_size = entry->GetDataSize(kSmallDataStream);
  int large_size = entry->GetDataSize(kLargeDataStream);
  scoped_refptr<net::IOBufferWithSize> small_buffer;
  scoped_refptr<BigIOBuffer> large_buffer;
  if (op->operation() == Operation::kFetch) {
    small_buffer = base::MakeRefCounted<net::IOBufferWithSize>(small_size);
    op->set_small_buffer(small_buffer);
    large_buffer = base::MakeRefCounted<BigIOBuffer>(large_size);
    op->set_large_buffer(large_buffer);
  } else {
    small_buffer = op->small_buffer();
    large_buffer = op->large_buffer();
    DCHECK_EQ(small_size, small_buffer->size());
    DCHECK_EQ(large_size, large_buffer->size());
  }

  // Read the small data first.
  int result = entry->ReadData(
      kSmallDataStream, 0, small_buffer.get(), small_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::ReadSmallBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op));

  if (result != net::ERR_IO_PENDING) {
    ReadSmallBufferComplete(op, result);
  }

  // Skip the large read if data is in the small read.
  if (large_size == 0)
    return;

  // Read the large data.
  result = entry->ReadData(
      kLargeDataStream, 0, large_buffer.get(), large_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::ReadLargeBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op));
  if (result != net::ERR_IO_PENDING) {
    ReadLargeBufferComplete(op, result);
  }
}

void GeneratedCodeCache::ReadSmallBufferComplete(PendingOperation* op, int rv) {
  DCHECK(Operation::kFetch == op->operation() ||
         Operation::kFetchWithSHAKey == op->operation());
  bool no_header = op->operation() == Operation::kFetchWithSHAKey;
  bool succeeded = (rv == op->small_buffer()->size() &&
                    (no_header || IsValidHeader(op->small_buffer())));
  CollectStatistics(
      op->resource_url(), op->origin_lock(),
      succeeded ? CacheEntryStatus::kHit : CacheEntryStatus::kMiss);

  if (op->AddBufferCompletion(succeeded))
    ReadComplete(op);

  // Small reads must finish now since no large read is pending.
  if (op->large_buffer()->size() == 0)
    ReadLargeBufferComplete(op, 0);
}

void GeneratedCodeCache::ReadLargeBufferComplete(PendingOperation* op, int rv) {
  DCHECK(Operation::kFetch == op->operation() ||
         Operation::kFetchWithSHAKey == op->operation());
  if (op->AddBufferCompletion(rv == op->large_buffer()->size()))
    ReadComplete(op);
}

void GeneratedCodeCache::ReadComplete(PendingOperation* op) {
  DCHECK(Operation::kFetch == op->operation() ||
         Operation::kFetchWithSHAKey == op->operation());
  if (!op->succeeded()) {
    op->RunReadCallback(this, base::Time(), mojo_base::BigBuffer());
    // Doom this entry since it is inaccessible.
    DoomEntry(op);
  } else {
    if (op->operation() != Operation::kFetchWithSHAKey) {
      base::Time response_time;
      uint32_t data_size = 0;
      ReadCommonDataHeader(op->small_buffer().get(), &response_time,
                           &data_size);
      if (data_size <= kInlineDataLimit) {
        // Small data. Copy the data from the small buffer.
        DCHECK_EQ(0, op->large_buffer()->size());
        mojo_base::BigBuffer data(
            op->small_buffer()->span().subspan(kHeaderSizeInBytes, data_size));
        op->RunReadCallback(this, response_time, std::move(data));
      } else if (!ShouldDeduplicateEntry(data_size)) {
        // Large data below the merging threshold, or deduplication is disabled.
        // Return the large buffer.
        op->RunReadCallback(this, response_time,
                            op->large_buffer()->TakeBuffer());
      } else {
        // Very large data. Create the second fetch using the checksum as key.
        DCHECK_EQ(static_cast<int>(kHeaderSizeInBytes + kSHAKeySizeInBytes),
                  op->small_buffer()->size());
        std::string checksum_key(
            op->small_buffer()->data() + kHeaderSizeInBytes,
            kSHAKeySizeInBytes);
        auto small_buffer = base::MakeRefCounted<net::IOBufferWithSize>(0);
        auto large_buffer = base::MakeRefCounted<BigIOBuffer>(data_size);
        auto op2 = std::make_unique<PendingOperation>(
            Operation::kFetchWithSHAKey, op->resource_url(), op->origin_lock(),
            checksum_key, response_time, op->start_time(), small_buffer,
            large_buffer, op->TakeReadCallback());
        EnqueueOperation(std::move(op2));
      }
    } else {
      // Large merged code data with no header. |op| holds the response time.
      op->RunReadCallback(this, op->response_time(),
                          op->large_buffer()->TakeBuffer());
    }
  }
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::DeleteEntryImpl(PendingOperation* op) {
  DCHECK_EQ(Operation::kDelete, op->operation());
  DoomEntry(op);
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::DoomEntry(PendingOperation* op) {
  // Write, Fetch, and Delete may all doom an entry.
  DCHECK_NE(Operation::kGetBackend, op->operation());
  // Entries shouldn't be doomed if the backend hasn't been initialized.
  DCHECK_EQ(kInitialized, backend_state_);
  CollectStatistics(op->resource_url(), op->origin_lock(),
                    CacheEntryStatus::kClear);
  backend_->DoomEntry(op->key(), net::LOWEST, net::CompletionOnceCallback());
}

void GeneratedCodeCache::IssueNextOperation(const std::string& key) {
  auto it = active_entries_map_.find(key);
  if (it == active_entries_map_.end())
    return;

  DCHECK(!it->second.empty());
  IssueOperation(it->second.front().get());
}

void GeneratedCodeCache::CloseOperationAndIssueNext(PendingOperation* op) {
  // Dequeue op, keeping it alive long enough to issue another op.
  std::unique_ptr<PendingOperation> keep_alive = DequeueOperation(op);
  IssueNextOperation(op->key());
}

void GeneratedCodeCache::EnqueueOperationAndIssueIfNext(
    std::unique_ptr<PendingOperation> op) {
  // GetBackend ops have no key and shouldn't be enqueued here.
  DCHECK_NE(Operation::kGetBackend, op->operation());
  auto it = active_entries_map_.find(op->key());
  bool can_issue = false;
  if (it == active_entries_map_.end()) {
    it = active_entries_map_.emplace(op->key(), PendingOperationQueue()).first;
    can_issue = true;
  }
  const std::string& key = op->key();
  it->second.emplace(std::move(op));
  if (can_issue)
    IssueNextOperation(key);
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::DequeueOperation(PendingOperation* op) {
  auto it = active_entries_map_.find(op->key());
  CHECK(it != active_entries_map_.end(), base::NotFatalUntil::M130);
  DCHECK(!it->second.empty());
  std::unique_ptr<PendingOperation> result = std::move(it->second.front());
  // |op| should be at the front.
  DCHECK_EQ(op, result.get());
  it->second.pop();
  // Delete the queue if it becomes empty.
  if (it->second.empty()) {
    active_entries_map_.erase(it);
  }
  return result;
}

void GeneratedCodeCache::DoPendingGetBackend(PendingOperation* op) {
  // |op| is kept alive in |IssuePendingOperations| for the duration of this
  // call. We shouldn't access |op| after returning from this function.
  DCHECK_EQ(kGetBackend, op->operation());
  if (backend_state_ == kInitialized) {
    op->TakeBackendCallback().Run(backend_.get());
  } else {
    DCHECK_EQ(backend_state_, kFailed);
    op->TakeBackendCallback().Run(nullptr);
  }
}

bool GeneratedCodeCache::IsDeduplicationEnabled() const {
  // Deduplication is disabled in the WebUI code cache, as an additional defense
  // against privilege escalation in case there is a bug in the deduplication
  // logic.
  return cache_type_ != kWebUIJavaScript;
}

bool GeneratedCodeCache::ShouldDeduplicateEntry(uint32_t data_size) const {
  return data_size > kDedicatedDataLimit && IsDeduplicationEnabled();
}

void GeneratedCodeCache::SetLastUsedTimeForTest(
    const GURL& resource_url,
    const GURL& origin_lock,
    const net::NetworkIsolationKey& nik,
    base::Time time,
    base::OnceClosure user_callback) {
  // This is used only for tests. So reasonable to assume that backend is
  // initialized here. All other operations handle the case when backend was not
  // yet opened.
  DCHECK_EQ(backend_state_, kInitialized);
  auto split = base::SplitOnceCallback(std::move(user_callback));

  disk_cache::EntryResultCallback callback = base::BindOnce(
      &GeneratedCodeCache::OpenCompleteForSetLastUsedForTest,
      weak_ptr_factory_.GetWeakPtr(), time, std::move(split.first));

  std::string key = GetCacheKey(resource_url, origin_lock, nik, cache_type_);
  disk_cache::EntryResult result =
      backend_->OpenEntry(key, net::LOWEST, std::move(callback));
  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForSetLastUsedForTest(time, std::move(split.second),
                                      std::move(result));
  }
}

void GeneratedCodeCache::ClearInMemoryCache() {
  lru_cache_.Clear();
}

void GeneratedCodeCache::OpenCompleteForSetLastUsedForTest(
    base::Time time,
    base::OnceClosure callback,
    disk_cache::EntryResult result) {
  DCHECK_EQ(result.net_error(), net::OK);
  {
    disk_cache::ScopedEntryPtr disk_entry(result.ReleaseEntry());
    DCHECK(disk_entry);
    disk_entry->SetLastUsedTimeForTest(time);
  }
  std::move(callback).Run();
}

void GeneratedCodeCache::CollectStatisticsForTest(
    const GURL& resource_url,
    const GURL& origin_lock,
    GeneratedCodeCache::CacheEntryStatus status) {
  CollectStatistics(resource_url, origin_lock, status);
}

}  // namespace content
