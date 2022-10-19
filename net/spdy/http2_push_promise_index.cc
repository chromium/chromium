// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/http2_push_promise_index.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace net {

Http2PushPromiseIndex::Http2PushPromiseIndex() = default;

Http2PushPromiseIndex::~Http2PushPromiseIndex() {
  DCHECK(unclaimed_pushed_streams_.empty());
}

bool Http2PushPromiseIndex::RegisterUnclaimedPushedStream(
    const GURL& url,
    spdy::SpdyStreamId stream_id,
    Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK_GT(stream_id, kNoPushedStreamFound);
  DCHECK(delegate);

  // Find the entry with |url| for |delegate| if such exists (there can be at
  // most one such entry).  It is okay to cast away const from |delegate|,
  // because it is only used for lookup.
  auto it = unclaimed_pushed_streams_.lower_bound(UnclaimedPushedStream{
      url, const_cast<Delegate*>(delegate), kNoPushedStreamFound});
  // If such entry is found, do not allow registering another one.
  if (it != unclaimed_pushed_streams_.end() && it->url == url &&
      it->delegate == delegate) {
    return false;
  }

  unclaimed_pushed_streams_.insert(
      it, UnclaimedPushedStream{url, delegate, stream_id});

  return true;
}

bool Http2PushPromiseIndex::UnregisterUnclaimedPushedStream(
    const GURL& url,
    spdy::SpdyStreamId stream_id,
    Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK_GT(stream_id, kNoPushedStreamFound);
  DCHECK(delegate);

  size_t result = unclaimed_pushed_streams_.erase(
      UnclaimedPushedStream{url, delegate, stream_id});

  return result == 1;
}

// The runtime of this method is linear in unclaimed_pushed_streams_.size(),
// which is acceptable, because it is only used in NetLog, tests, and DCHECKs.
size_t Http2PushPromiseIndex::CountStreamsForSession(
    const Delegate* delegate) const {
  DCHECK(delegate);

  return base::ranges::count(unclaimed_pushed_streams_, delegate,
                             &UnclaimedPushedStream::delegate);
}

spdy::SpdyStreamId Http2PushPromiseIndex::FindStream(
    const GURL& url,
    const Delegate* delegate) const {
  // Find the entry with |url| for |delegate| if such exists (there can be at
  // most one such entry).  It is okay to cast away const from |delegate|,
  // because it is only used for lookup.
  auto it = unclaimed_pushed_streams_.lower_bound(UnclaimedPushedStream{
      url, const_cast<Delegate*>(delegate), kNoPushedStreamFound});

  if (it == unclaimed_pushed_streams_.end() || it->url != url ||
      it->delegate != delegate) {
    return kNoPushedStreamFound;
  }

  return it->stream_id;
}

void Http2PushPromiseIndex::ClaimPushedStream(
    const SpdySessionKey& key,
    const GURL& url,
    const HttpRequestInfo& request_info,
    base::WeakPtr<SpdySession>* session,
    spdy::SpdyStreamId* stream_id) {
  DCHECK(!url.is_empty());

  *session = nullptr;
  *stream_id = kNoPushedStreamFound;

  // Find the first entry for |url|, if such exists.
  auto it = unclaimed_pushed_streams_.lower_bound(
      UnclaimedPushedStream{url, nullptr, kNoPushedStreamFound});

  while (it != unclaimed_pushed_streams_.end() && it->url == url) {
    if (it->delegate->ValidatePushedStream(it->stream_id, url, request_info,
                                           key)) {
      *session = it->delegate->GetWeakPtrToSession();
      *stream_id = it->stream_id;
      unclaimed_pushed_streams_.erase(it);
      return;
    }
    ++it;
  }
}

bool Http2PushPromiseIndex::CompareByUrl::operator()(
    const UnclaimedPushedStream& a,
    const UnclaimedPushedStream& b) const {
  // Compare by URL first.
  if (a.url < b.url)
    return true;
  if (a.url > b.url)
    return false;
  // For identical URL, put an entry with delegate == nullptr first.
  // The C++ standard dictates that comparisons between |nullptr| and other
  // pointers are unspecified, hence the need to handle this case separately.
  if (a.delegate == nullptr && b.delegate != nullptr) {
    return true;
  }
  if (a.delegate != nullptr && b.delegate == nullptr) {
    return false;
  }
  // Then compare by Delegate.
  // The C++ standard guarantees that both |nullptr < nullptr| and
  // |nullptr > nullptr| are false, so there is no need to handle that case
  // separately.
  if (a.delegate < b.delegate)
    return true;
  if (a.delegate > b.delegate)
    return false;
  // If URL and Delegate are identical, then compare by stream ID.
  return a.stream_id < b.stream_id;
}

}  // namespace net
