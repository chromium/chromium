// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "base/notimplemented.h"
#include "net/base/net_errors.h"

namespace disk_cache {

SqlBackendImpl::SqlBackendImpl(net::CacheType cache_type)
    : Backend(cache_type) {}

SqlBackendImpl::~SqlBackendImpl() = default;

int64_t SqlBackendImpl::MaxFileSize() const {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int32_t SqlBackendImpl::GetEntryCount(
    net::Int32CompletionOnceCallback callback) const {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

EntryResult SqlBackendImpl::OpenOrCreateEntry(const std::string& key,
                                              net::RequestPriority priority,
                                              EntryResultCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return EntryResult::MakeError(net::ERR_NOT_IMPLEMENTED);
}

EntryResult SqlBackendImpl::OpenEntry(const std::string& key,
                                      net::RequestPriority priority,
                                      EntryResultCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return EntryResult::MakeError(net::ERR_NOT_IMPLEMENTED);
}

EntryResult SqlBackendImpl::CreateEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        EntryResultCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return EntryResult::MakeError(net::ERR_NOT_IMPLEMENTED);
}

net::Error SqlBackendImpl::DoomEntry(const std::string& key,
                                     net::RequestPriority priority,
                                     CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

net::Error SqlBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

net::Error SqlBackendImpl::DoomEntriesBetween(base::Time initial_time,
                                              base::Time end_time,
                                              CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

net::Error SqlBackendImpl::DoomEntriesSince(base::Time initial_time,
                                            CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int64_t SqlBackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int64_t SqlBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

std::unique_ptr<Backend::Iterator> SqlBackendImpl::CreateIterator() {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return nullptr;
}

void SqlBackendImpl::GetStats(base::StringPairs* stats) {
  stats->emplace_back(std::make_pair("Cache type", "SQL Cache"));
  // TODO(crbug.com/422065015): Write more stats.
}

void SqlBackendImpl::OnExternalCacheHit(const std::string& key) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
}

}  // namespace disk_cache
