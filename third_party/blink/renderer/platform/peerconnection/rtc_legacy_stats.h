// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_LEGACY_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_LEGACY_STATS_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/legacy_stats_types.h"

namespace blink {

class RTCLegacyStatsMemberIterator;

// TODO(crbug.com/787254): Remove both RTCLegacyStats and
// RTCLegacyStatsMemberIterator base interfaces when they stopped
// being referenced by renderer/platform (namely rtc_stats_response_base.h).
class RTCLegacyStats {
 public:
  virtual ~RTCLegacyStats() = default;

  virtual String Id() const = 0;
  virtual String GetType() const = 0;
  virtual double Timestamp() const = 0;

  // The caller owns the iterator. The iterator must not be used after
  // the |RTCLegacyStats| that created it is destroyed.
  virtual RTCLegacyStatsMemberIterator* Iterator() const = 0;
};

class RTCLegacyStatsMemberIterator {
 public:
  virtual ~RTCLegacyStatsMemberIterator() = default;
  virtual bool IsEnd() const = 0;
  virtual void Next() = 0;

  virtual String GetName() const = 0;
  virtual webrtc::StatsReport::Value::Type GetType() const = 0;
  // Value getters. No conversion is performed; the function must match the
  // member's |type|.
  virtual int ValueInt() const = 0;
  virtual int64_t ValueInt64() const = 0;
  virtual float ValueFloat() const = 0;
  virtual String ValueString() const = 0;
  virtual bool ValueBool() const = 0;

  // Converts the value to string (regardless of |type|).
  virtual String ValueToString() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_LEGACY_STATS_H_
