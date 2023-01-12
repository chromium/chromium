// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_collector_callback.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace webrtc {
class RTCStatsCollectorCallback;
enum class NonStandardGroupId;
}  // namespace webrtc

namespace blink {

class RTCStats;
class RTCStatsMember;

PLATFORM_EXPORT BASE_DECLARE_FEATURE(WebRtcUnshipDeprecatedStats);

// Wrapper around a webrtc::RTCStatsReport. Filters out any stats objects that
// aren't listed in the allow list. |filter| controls whether to include only
// standard members (RTCStatsMemberInterface::is_standardized return true) or
// not (RTCStatsMemberInterface::is_standardized return false).
//
// Note: This class is named |RTCStatsReportPlatform| not to collide with class
// |RTCStatsReport|, from renderer/modules/peerconnection/rtc_stats_report.cc|h.
class PLATFORM_EXPORT RTCStatsReportPlatform {
 public:
  RTCStatsReportPlatform(
      const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
      const Vector<webrtc::NonStandardGroupId>& exposed_group_ids);
  virtual ~RTCStatsReportPlatform();
  // Creates a new report object that is a handle to the same underlying stats
  // report (the stats are not copied). The new report's iterator is reset,
  // useful when needing multiple iterators.
  std::unique_ptr<RTCStatsReportPlatform> CopyHandle() const;

  // Gets stats object by |id|, or null if no stats with that |id| exists.
  std::unique_ptr<RTCStats> GetStats(const String& id) const;

  // The next stats object, or null if the end has been reached.
  std::unique_ptr<RTCStats> Next();

  // The number of stats objects.
  size_t Size() const;

 private:
  const bool unship_deprecated_stats_;
  const scoped_refptr<const webrtc::RTCStatsReport> stats_report_;
  webrtc::RTCStatsReport::ConstIterator it_;
  const webrtc::RTCStatsReport::ConstIterator end_;
  Vector<webrtc::NonStandardGroupId> exposed_group_ids_;
  // Number of allowlisted webrtc::RTCStats in |stats_report_|.
  const size_t size_;
};

class PLATFORM_EXPORT RTCStats {
 public:
  RTCStats(const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
           const webrtc::RTCStats* stats,
           const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
           bool unship_deprecated_stats);
  virtual ~RTCStats();

  String Id() const;
  String GetType() const;
  double TimestampMs() const;

  size_t MembersCount() const;
  std::unique_ptr<RTCStatsMember> GetMember(size_t i) const;

 private:
  // Reference to keep the report that owns |stats_| alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStats* const stats_;
  // Members of the |stats_| object, equivalent to |stats_->Members()|.
  const std::vector<const webrtc::RTCStatsMemberInterface*> stats_members_
      ALLOW_DISCOURAGED_TYPE("Matches webrtc::RTCStats::Members()");
};

class PLATFORM_EXPORT RTCStatsMember {
 public:
  RTCStatsMember(const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
                 const webrtc::RTCStatsMemberInterface* member);
  virtual ~RTCStatsMember();

  String GetName() const;
  webrtc::RTCStatsMemberInterface::Type GetType() const;
  bool IsDefined() const;

  bool ValueBool() const;
  int32_t ValueInt32() const;
  uint32_t ValueUint32() const;
  int64_t ValueInt64() const;
  uint64_t ValueUint64() const;
  double ValueDouble() const;
  String ValueString() const;
  Vector<bool> ValueSequenceBool() const;
  Vector<int32_t> ValueSequenceInt32() const;
  Vector<uint32_t> ValueSequenceUint32() const;
  Vector<int64_t> ValueSequenceInt64() const;
  Vector<uint64_t> ValueSequenceUint64() const;
  Vector<double> ValueSequenceDouble() const;
  Vector<String> ValueSequenceString() const;
  HashMap<String, uint64_t> ValueMapStringUint64() const;
  HashMap<String, double> ValueMapStringDouble() const;

  enum class ExposureRestriction { kNone, kHardwareCapability };
  ExposureRestriction Restriction() const;

 private:
  // Reference to keep the report that owns |member_|'s stats object alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to member of a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStatsMemberInterface* const member_;
};

using RTCStatsReportCallback =
    base::OnceCallback<void(std::unique_ptr<RTCStatsReportPlatform>)>;

PLATFORM_EXPORT
rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids);

// A stats collector callback.
// It is invoked on the WebRTC signaling thread and will post a task to invoke
// |callback| on the thread given in the |main_thread| argument.
// The argument to the callback will be a |RTCStatsReportPlatform|.
class PLATFORM_EXPORT RTCStatsCollectorCallbackImpl
    : public webrtc::RTCStatsCollectorCallback {
 public:
  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

 protected:
  RTCStatsCollectorCallbackImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      RTCStatsReportCallback callback,
      const Vector<webrtc::NonStandardGroupId>& exposed_group_ids);
  ~RTCStatsCollectorCallbackImpl() override;

  void OnStatsDeliveredOnMainThread(
      rtc::scoped_refptr<const webrtc::RTCStatsReport> report);

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  RTCStatsReportCallback callback_;
  Vector<webrtc::NonStandardGroupId> exposed_group_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_
