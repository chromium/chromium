// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/url_index.h"

#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "third_party/blink/renderer/platform/media/resource_multi_buffer_data_provider.h"

namespace blink {

const int kBlockSizeShift = 15;  // 1<<15 == 32kb
const int kUrlMappingTimeoutSeconds = 300;

ResourceMultiBuffer::ResourceMultiBuffer(
    UrlData* url_data,
    int block_shift,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : MultiBuffer(block_shift, url_data->url_index_->lru_),
      url_data_(url_data),
      task_runner_(std::move(task_runner)) {}

ResourceMultiBuffer::~ResourceMultiBuffer() = default;

std::unique_ptr<MultiBuffer::DataProvider> ResourceMultiBuffer::CreateWriter(
    const MultiBufferBlockId& pos,
    bool is_client_audio_element) {
  auto writer = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_, pos, is_client_audio_element, task_runner_);
  writer->Start();
  return writer;
}

bool ResourceMultiBuffer::RangeSupported() const {
  return url_data_->range_supported_;
}

void ResourceMultiBuffer::OnEmpty() {
  url_data_->OnEmpty();
}

UrlData::UrlData(base::PassKey<UrlIndex>,
                 const GURL& url,
                 CorsMode cors_mode,
                 UrlIndex* url_index,
                 CacheMode cache_lookup_mode,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UrlData(url,
              cors_mode,
              url_index,
              cache_lookup_mode,
              std::move(task_runner)) {}

UrlData::UrlData(const GURL& url,
                 CorsMode cors_mode,
                 UrlIndex* url_index,
                 CacheMode cache_lookup_mode,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : url_(url),
      have_data_origin_(false),
      cors_mode_(cors_mode),
      has_access_control_(false),
      url_index_(url_index),
      length_(kPositionNotSpecified),
      range_supported_(false),
      cacheable_(false),
      cache_lookup_mode_(cache_lookup_mode),
      multibuffer_(this, url_index_->block_shift_, std::move(task_runner)) {}

UrlData::~UrlData() = default;

std::pair<GURL, UrlData::CorsMode> UrlData::key() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_pair(url(), cors_mode());
}

void UrlData::set_valid_until(base::Time valid_until) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  valid_until_ = valid_until;
}

void UrlData::MergeFrom(const scoped_refptr<UrlData>& other) {
  // We're merging from another UrlData that refers to the *same*
  // resource, so when we merge the metadata, we can use the most
  // optimistic values.
  if (ValidateDataOrigin(other->data_origin_)) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    valid_until_ = std::max(valid_until_, other->valid_until_);
    // set_length() will not override the length if already known.
    set_length(other->length_);
    cacheable_ |= other->cacheable_;
    cache_lookup_mode_ = other->cache_lookup_mode_;
    range_supported_ |= other->range_supported_;
    if (last_modified_.is_null()) {
      last_modified_ = other->last_modified_;
    }
    bytes_read_from_cache_ += other->bytes_read_from_cache_;
    // is_cors_corss_origin_ will not relax from true to false.
    set_is_cors_cross_origin(other->is_cors_cross_origin_);
    has_access_control_ |= other->has_access_control_;
    multibuffer()->MergeFrom(other->multibuffer());
  }
}

void UrlData::set_cacheable(bool cacheable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cacheable_ = cacheable;
}

void UrlData::set_length(int64_t length) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (length != kPositionNotSpecified) {
    length_ = length;
  }
}

void UrlData::set_is_cors_cross_origin(bool is_cors_cross_origin) {
  if (is_cors_cross_origin_)
    return;
  is_cors_cross_origin_ = is_cors_cross_origin;
}

void UrlData::set_has_access_control() {
  has_access_control_ = true;
}

void UrlData::set_mime_type(std::string mime_type) {
  mime_type_ = std::move(mime_type);
}

void UrlData::set_passed_timing_allow_origin_check(
    bool passed_timing_allow_origin_check) {
  passed_timing_allow_origin_check_ = passed_timing_allow_origin_check;
}

void UrlData::RedirectTo(const scoped_refptr<UrlData>& url_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Copy any cached data over to the new location.
  url_data->multibuffer()->MergeFrom(multibuffer());

  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (RedirectCB& cb : redirect_callbacks) {
    std::move(cb).Run(url_data);
  }
}

void UrlData::Fail() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Handled similar to a redirect.
  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (RedirectCB& cb : redirect_callbacks) {
    std::move(cb).Run(nullptr);
  }
}

void UrlData::OnRedirect(RedirectCB cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  redirect_callbacks_.push_back(std::move(cb));
}

void UrlData::Use() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  last_used_ = base::Time::Now();
}

bool UrlData::ValidateDataOrigin(const GURL& origin) {
  if (!have_data_origin_) {
    data_origin_ = origin;
    have_data_origin_ = true;
    return true;
  }
  if (cors_mode_ == UrlData::CORS_UNSPECIFIED) {
    return data_origin_ == origin;
  }
  // The actual cors checks is done in the net layer.
  return true;
}

void UrlData::OnEmpty() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  url_index_->RemoveUrlData(this);
}

bool UrlData::FullyCached() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (length_ == kPositionNotSpecified)
    return false;
  // Check that the first unavailable block in the cache is after the
  // end of the file.
  return (multibuffer()->FindNextUnavailable(0) << kBlockSizeShift) >= length_;
}

bool UrlData::Valid() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Time now = base::Time::Now();
  if (!range_supported_ && !FullyCached())
    return false;
  // When ranges are not supported, we cannot re-use cached data.
  if (valid_until_ > now)
    return true;
  if (now - last_used_ < base::Seconds(kUrlMappingTimeoutSeconds))
    return true;
  return false;
}

void UrlData::set_last_modified(base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  last_modified_ = last_modified;
}

void UrlData::set_etag(const std::string& etag) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  etag_ = etag;
}

void UrlData::set_range_supported() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  range_supported_ = true;
}

ResourceMultiBuffer* UrlData::multibuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return &multibuffer_;
}

size_t UrlData::CachedSize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return multibuffer()->map().size();
}

UrlIndex::UrlIndex(ResourceFetchContext* fetch_context,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UrlIndex(fetch_context, kBlockSizeShift, std::move(task_runner)) {}

UrlIndex::UrlIndex(ResourceFetchContext* fetch_context,
                   int block_shift,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : fetch_context_(fetch_context),
      lru_(base::MakeRefCounted<MultiBuffer::GlobalLRU>(task_runner)),
      block_shift_(block_shift),
      memory_pressure_listener_(FROM_HERE,
                                base::BindRepeating(&UrlIndex::OnMemoryPressure,
                                                    base::Unretained(this))),
      task_runner_(std::move(task_runner)) {}

UrlIndex::~UrlIndex() {
#if DCHECK_IS_ON()
  // Verify that only |this| holds reference to UrlData instances.
  auto dcheck_has_one_ref = [](const UrlDataMap::value_type& entry) {
    DCHECK(entry.second->HasOneRef());
  };
  base::ranges::for_each(indexed_data_, dcheck_has_one_ref);
#endif
}

void UrlIndex::RemoveUrlData(const scoped_refptr<UrlData>& url_data) {
  DCHECK(url_data->multibuffer()->map().empty());

  auto i = indexed_data_.find(url_data->key());
  if (i != indexed_data_.end() && i->second == url_data)
    indexed_data_.erase(i);
}

scoped_refptr<UrlData> UrlIndex::GetByUrl(const GURL& gurl,
                                          UrlData::CorsMode cors_mode,
                                          UrlData::CacheMode cache_mode) {
  if (cache_mode == UrlData::kNormal) {
    auto i = indexed_data_.find(std::make_pair(gurl, cors_mode));
    if (i != indexed_data_.end() && i->second->Valid()) {
      return i->second;
    }
  }

  return NewUrlData(gurl, cors_mode, cache_mode);
}

scoped_refptr<UrlData> UrlIndex::NewUrlData(
    const GURL& url,
    UrlData::CorsMode cors_mode,
    UrlData::CacheMode cache_lookup_mode) {
  return base::MakeRefCounted<UrlData>(base::PassKey<UrlIndex>(), url,
                                       cors_mode, this, cache_lookup_mode,
                                       task_runner_);
}

void UrlIndex::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      lru_->TryFree(128);  // try to free 128 32kb blocks if possible
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      lru_->TryFreeAll();  // try to free as many blocks as possible
      break;
  }
}

namespace {
bool IsStrongEtag(const std::string& etag) {
  return etag.size() > 2 && etag[0] == '"';
}

bool IsNewDataForSameResource(const scoped_refptr<UrlData>& new_entry,
                              const scoped_refptr<UrlData>& old_entry) {
  if (IsStrongEtag(new_entry->etag()) && IsStrongEtag(old_entry->etag())) {
    if (new_entry->etag() != old_entry->etag())
      return true;
  }
  if (!new_entry->last_modified().is_null()) {
    if (new_entry->last_modified() != old_entry->last_modified())
      return true;
  }
  return false;
}
}  // namespace

scoped_refptr<UrlData> UrlIndex::TryInsert(
    const scoped_refptr<UrlData>& url_data) {
  auto iter = indexed_data_.find(url_data->key());
  if (iter == indexed_data_.end()) {
    // If valid and not already indexed, index it.
    if (url_data->Valid()) {
      indexed_data_.insert(iter, std::make_pair(url_data->key(), url_data));
    }
    return url_data;
  }

  // A UrlData instance for the same key is already indexed.

  // If the indexed instance is the same as |url_data|,
  // nothing needs to be done.
  if (iter->second == url_data)
    return url_data;

  // The indexed instance is different.
  // Check if it should be replaced with |url_data|.
  if (IsNewDataForSameResource(url_data, iter->second)) {
    if (url_data->Valid()) {
      iter->second = url_data;
    }
    return url_data;
  }

  // If the url data should bypass the cache lookup, we want to not merge it.
  if (url_data->cache_lookup_mode() == UrlData::kCacheDisabled) {
    return url_data;
  }

  if (url_data->Valid()) {
    if ((!iter->second->Valid() ||
         url_data->CachedSize() > iter->second->CachedSize())) {
      iter->second = url_data;
    } else {
      iter->second->MergeFrom(url_data);
    }
  }
  return iter->second;
}

}  // namespace blink
