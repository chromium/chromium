// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_registry.h"

#include "content/browser/streams/stream.h"

namespace content {

namespace {
// The maximum size of memory each StreamRegistry instance is allowed to use
// for its Stream instances.
const size_t kDefaultMaxMemoryUsage = 1024 * 1024 * 1024U;  // 1GiB
}

StreamRegistry::StreamRegistry()
    : total_memory_usage_(0),
      max_memory_usage_(kDefaultMaxMemoryUsage) {
}

StreamRegistry::~StreamRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(register_observers_.empty());
}

void StreamRegistry::RegisterStream(Stream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream);
  DCHECK(!stream->url().is_empty());

  auto aborted_url_itr = reader_aborted_urls_.find(stream->url());
  if (aborted_url_itr != reader_aborted_urls_.end()) {
    reader_aborted_urls_.erase(aborted_url_itr);
    return;
  }
  streams_[stream->url()] = stream;

  auto itr = register_observers_.find(stream->url());
  if (itr != register_observers_.end())
    itr->second->OnStreamRegistered(stream);
}

scoped_refptr<Stream> StreamRegistry::GetStream(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StreamMap::const_iterator stream = streams_.find(url);
  if (stream != streams_.end())
    return stream->second;

  return nullptr;
}

bool StreamRegistry::CloneStream(const GURL& url, const GURL& src_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<Stream> stream(GetStream(src_url));
  if (stream.get()) {
    streams_[url] = stream;
    return true;
  }
  return false;
}

void StreamRegistry::UnregisterStream(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = streams_.find(url);
  if (iter == streams_.end())
    return;

  // Only update |total_memory_usage_| if |url| is NOT a Stream clone because
  // cloned streams do not update |total_memory_usage_|.
  if (iter->second->url() == url) {
    size_t buffered_bytes = iter->second->last_total_buffered_bytes();
    DCHECK_LE(buffered_bytes, total_memory_usage_);
    total_memory_usage_ -= buffered_bytes;
  }

  streams_.erase(url);
}

bool StreamRegistry::UpdateMemoryUsage(const GURL& url,
                                       size_t current_size,
                                       size_t increase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = streams_.find(url);
  // A Stream must be registered with its parent registry to get memory.
  if (iter == streams_.end())
    return false;

  size_t last_size = iter->second->last_total_buffered_bytes();
  DCHECK_LE(last_size, total_memory_usage_);
  size_t usage_of_others = total_memory_usage_ - last_size;
  DCHECK_LE(current_size, last_size);
  size_t current_total_memory_usage = usage_of_others + current_size;

  if (increase > max_memory_usage_ - current_total_memory_usage)
    return false;

  total_memory_usage_ = current_total_memory_usage + increase;
  return true;
}


void StreamRegistry::SetRegisterObserver(const GURL& url,
                                         StreamRegisterObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(register_observers_.find(url) == register_observers_.end());
  register_observers_[url] = observer;
}

void StreamRegistry::RemoveRegisterObserver(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  register_observers_.erase(url);
}

void StreamRegistry::AbortPendingStream(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reader_aborted_urls_.insert(url);
}

}  // namespace content
