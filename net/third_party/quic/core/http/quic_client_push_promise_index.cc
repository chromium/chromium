// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_client_push_promise_index.h"

#include "net/third_party/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/platform/api/quic_string.h"

using spdy::SpdyHeaderBlock;

namespace quic {

QuicClientPushPromiseIndex::QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::~QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::TryHandle::~TryHandle() {}

QuicClientPromisedInfo* QuicClientPushPromiseIndex::GetPromised(
    const QuicString& url) {
  auto it = promised_by_url_.find(url);
  if (it == promised_by_url_.end()) {
    return nullptr;
  }
  return it->second;
}

QuicAsyncStatus QuicClientPushPromiseIndex::Try(
    const spdy::SpdyHeaderBlock& request,
    QuicClientPushPromiseIndex::Delegate* delegate,
    TryHandle** handle) {
  QuicString url(SpdyUtils::GetPromisedUrlFromHeaders(request));
  auto it = promised_by_url_.find(url);
  if (it != promised_by_url_.end()) {
    QuicClientPromisedInfo* promised = it->second;
    QuicAsyncStatus rv = promised->HandleClientRequest(request, delegate);
    if (rv == QUIC_PENDING) {
      *handle = promised;
    }
    return rv;
  }
  return QUIC_FAILURE;
}

}  // namespace quic
