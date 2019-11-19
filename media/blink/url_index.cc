// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/url_index.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "media/blink/resource_multibuffer_data_provider.h"

namespace media {

const int kBlockSizeShift = 15;  // 1<<15 == 32kb
const int kUrlMappingTimeoutSeconds = 300;

ResourceMultiBuffer::ResourceMultiBuffer(UrlData* url_data, int block_shift)
    : MultiBuffer(block_shift, url_data->url_index_->lru_),
      url_data_(url_data) {}

ResourceMultiBuffer::~ResourceMultiBuffer() = default;

std::unique_ptr<MultiBuffer::DataProvider> ResourceMultiBuffer::CreateWriter(
    const MultiBufferBlockId& pos,
    bool is_client_audio_element) {
  auto writer = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_, pos, is_client_audio_element);
  writer->Start();
  return writer;
}

bool ResourceMultiBuffer::RangeSupported() const {
  return url_data_->range_supported_;
}

void ResourceMultiBuffer::OnEmpty() {
  url_data_->OnEmpty();
}

UrlData::UrlData(const GURL& url, CorsMode cors_mode, UrlIndex* url_index)
    : url_(url),
      have_data_origin_(false),
      cors_mode_(cors_mode),
      has_access_control_(false),
      url_index_(url_index),
      length_(kPositionNotSpecified),
      range_supported_(false),
      cacheable_(false),
      last_used_(),
      multibuffer_(this, url_index_->block_shift_) {}

UrlData::~UrlData() = default;

std::pair<GURL, UrlData::CorsMode> UrlData::key() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_pair(url(), cors_mode());
}

void UrlData::set_valid_until(base::Time valid_until) {
  DCHECK(thread_checker_.CalledOnValidThread());
  valid_until_ = valid_until;
}

void UrlData::MergeFrom(const scoped_refptr<UrlData>& other) {
  // We're merging from another UrlData that refers to the *same*
  // resource, so when we merge the metadata, we can use the most
  // optimistic values.
  if (ValidateDataOrigin(other->data_origin_)) {
    DCHECK(thread_checker_.CalledOnValidThread());
    valid_until_ = std::max(valid_until_, other->valid_until_);
    // set_length() will not override the length if already known.
    set_length(other->length_);
    cacheable_ |= other->cacheable_;
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
  DCHECK(thread_checker_.CalledOnValidThread());
  cacheable_ = cacheable;
}

void UrlData::set_length(int64_t length) {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void UrlData::RedirectTo(const scoped_refptr<UrlData>& url_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Copy any cached data over to the new location.
  url_data->multibuffer()->MergeFrom(multibuffer());

  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (const RedirectCB& cb : redirect_callbacks) {
    cb.Run(url_data);
  }
}

void UrlData::Fail() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Handled similar to a redirect.
  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (const RedirectCB& cb : redirect_callbacks) {
    cb.Run(nullptr);
  }
}

void UrlData::OnRedirect(const RedirectCB& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  redirect_callbacks_.push_back(cb);
}

void UrlData::Use() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
  url_index_->RemoveUrlData(this);
}

bool UrlData::FullyCached() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (length_ == kPositionNotSpecified)
    return false;
  // Check that the first unavailable block in the cache is after the
  // end of the file.
  return (multibuffer()->FindNextUnavailable(0) << kBlockSizeShift) >= length_;
}

bool UrlData::Valid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Time now = base::Time::Now();
  if (!range_supported_ && !FullyCached())
    return false;
  // When ranges are not supported, we cannot re-use cached data.
  if (valid_until_ > now)
    return true;
  if (now - last_used_ <
      base::TimeDelta::FromSeconds(kUrlMappingTimeoutSeconds))
    return true;
  return false;
}

void UrlData::set_last_modified(base::Time last_modified) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_modified_ = last_modified;
}

void UrlData::set_etag(const std::string& etag) {
  DCHECK(thread_checker_.CalledOnValidThread());
  etag_ = etag;
}

void UrlData::set_range_supported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  range_supported_ = true;
}

ResourceMultiBuffer* UrlData::multibuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &multibuffer_;
}

size_t UrlData::CachedSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return multibuffer()->map().size();
}

UrlIndex::UrlIndex(ResourceFetchContext* fetch_context)
    : UrlIndex(fetch_context, kBlockSizeShift) {}

UrlIndex::UrlIndex(ResourceFetchContext* fetch_context, int block_shift)
    : fetch_context_(fetch_context),
      lru_(new MultiBuffer::GlobalLRU(base::ThreadTaskRunnerHandle::Get())),
      block_shift_(block_shift),
      memory_pressure_listener_(
          base::Bind(&UrlIndex::OnMemoryPressure, base::Unretained(this))) {}

UrlIndex::~UrlIndex() {
#if DCHECK_IS_ON()
  // Verify that only |this| holds reference to UrlData instances.
  auto dcheck_has_one_ref = [](const UrlDataMap::value_type& entry) {
    DCHECK(entry.second->HasOneRef());
  };
  std::for_each(indexed_data_.begin(), indexed_data_.end(), dcheck_has_one_ref);
#endif
}

void UrlIndex::RemoveUrlData(const scoped_refptr<UrlData>& url_data) {
  DCHECK(url_data->multibuffer()->map().empty());

  auto i = indexed_data_.find(url_data->key());
  if (i != indexed_data_.end() && i->second == url_data)
    indexed_data_.erase(i);
}

scoped_refptr<UrlData> UrlIndex::GetByUrl(const GURL& gurl,
                                          UrlData::CorsMode cors_mode) {
  auto i = indexed_data_.find(std::make_pair(gurl, cors_mode));
  if (i != indexed_data_.end() && i->second->Valid()) {
    return i->second;
  }

  return NewUrlData(gurl, cors_mode);
}

scoped_refptr<UrlData> UrlIndex::NewUrlData(const GURL& url,
                                            UrlData::CorsMode cors_mode) {
  return new UrlData(url, cors_mode, this);
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

}  // namespace media
