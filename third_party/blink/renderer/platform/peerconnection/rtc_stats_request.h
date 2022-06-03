/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_REQUEST_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaStreamComponent;
class RTCStatsResponseBase;

// TODO(crbug.com/787254): Merge RTCStatsRequest and RTCStatsRequestImpl
// when the former is not referenced in renderer/platform anymore.
//
// The RTCStatsRequest class represents a JavaScript call on
// RTCPeerConnection.getStats(). The user of this API will use
// the calls on this class and RTCStatsResponseBase to fill in the
// data that will be returned via a callback to the user in an
// RTCStatsResponse structure.
//
// The typical usage pattern is:
// RTCStatsRequest* request = <from somewhere>
// RTCStatsResponseBase* response = request->CreateResponse();
//
// For each item on which statistics are going to be reported:
//   RTCLegacyStats stats(...);
//   (configuration of stats object depends on item type)
//   response.AddStats(stats);
// When finished adding information:
// request->RequestSucceeded(response);
class RTCStatsRequest : public GarbageCollected<RTCStatsRequest> {
 public:
  virtual ~RTCStatsRequest() = default;

  virtual RTCStatsResponseBase* CreateResponse() = 0;

  // This function returns true if a selector argument was given to getStats.
  virtual bool HasSelector() = 0;

  // The Component() accessor give the information
  // required to look up a MediaStreamTrack implementation.
  // It is only useful to call it when HasSelector() returns true.
  virtual MediaStreamComponent* Component() = 0;
  virtual void RequestSucceeded(RTCStatsResponseBase*) = 0;

  virtual void Trace(Visitor* visitor) const {}

 protected:
  RTCStatsRequest() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_REQUEST_H_
