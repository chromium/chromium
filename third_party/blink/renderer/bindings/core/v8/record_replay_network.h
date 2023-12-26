// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_NETWORK_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_NETWORK_H_

// Methods for notifying the recorder about activity related to network requests.

#include "base/record_replay.h"

namespace blink {

class Document;
class KURL;
class Resource;
class ResourceRequest;
class ResourceResponse;
struct WebURLError;

} // namespace blink

namespace recordreplay {

void OnNetworkPrepareRequest(const blink::Document* document,
                             const blink::Resource* resource,
                             const blink::ResourceRequest& request);

void OnNetworkResourceRedirect(uint64_t inspector_id, const blink::KURL& new_url,
                               blink::ResourceRequest* new_request);

void OnNetworkReceiveResponse(uint64_t inspector_id,
                                 const blink::ResourceResponse& response);

void OnNetworkReceiveData(uint64_t inspector_id, const char* data, int length);

void OnNetworkFinishLoading(uint64_t inspector_id,
                               int64_t encoded_body_length,
                               int64_t decoded_body_length);

void OnNetworkFail(uint64_t inspector_id, const blink::WebURLError& error);

}  // namespace recordreplay

#endif // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_NETWORK_H_
