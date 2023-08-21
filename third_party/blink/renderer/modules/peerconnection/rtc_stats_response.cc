/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_response.h"

#include "base/containers/contains.h"

namespace blink {

RTCStatsResponse::RTCStatsResponse() = default;

RTCLegacyStatsReport* RTCStatsResponse::namedItem(const AtomicString& name) {
  if (base::Contains(idmap_, name)) {
    return result_[idmap_.at(name)];
  }
  return nullptr;
}

void RTCStatsResponse::AddStats(const RTCLegacyStats& stats) {
  result_.push_back(MakeGarbageCollected<RTCLegacyStatsReport>(
      stats.Id(), stats.GetType(), stats.Timestamp()));
  idmap_.insert(stats.Id(), result_.size() - 1);
  RTCLegacyStatsReport* report = result_[result_.size() - 1].Get();

  for (std::unique_ptr<RTCLegacyStatsMemberIterator> member(stats.Iterator());
       !member->IsEnd(); member->Next()) {
    report->AddStatistic(member->GetName(), member->ValueToString());
  }
}

void RTCStatsResponse::Trace(Visitor* visitor) const {
  visitor->Trace(result_);
  RTCStatsResponseBase::Trace(visitor);
}

}  // namespace blink
