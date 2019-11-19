// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

namespace WTF {

template <typename T>
struct CrossThreadCopier<rtc::scoped_refptr<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = rtc::scoped_refptr<T>;
  static Type Copy(Type pointer) { return pointer; }
};

}  // namespace WTF

namespace blink {

namespace {

class RTCStatsWhitelist {
 public:
  RTCStatsWhitelist() {
    whitelisted_stats_types_.insert(webrtc::RTCCertificateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCCodecStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCDataChannelStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCIceCandidatePairStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCLocalIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCRemoteIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCMediaStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCMediaStreamTrackStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCPeerConnectionStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCRTPStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCInboundRTPStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCOutboundRTPStreamStats::kType);
    whitelisted_stats_types_.insert(
        webrtc::RTCRemoteInboundRtpStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCMediaSourceStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCAudioSourceStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCVideoSourceStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCTransportStats::kType);
  }

  bool IsWhitelisted(const webrtc::RTCStats& stats) {
    return whitelisted_stats_types_.find(stats.type()) !=
           whitelisted_stats_types_.end();
  }

  void WhitelistStatsForTesting(const char* type) {
    whitelisted_stats_types_.insert(type);
  }

 private:
  std::set<std::string> whitelisted_stats_types_;
};

RTCStatsWhitelist* GetStatsWhitelist() {
  static RTCStatsWhitelist* whitelist = new RTCStatsWhitelist();
  return whitelist;
}

bool IsWhitelistedStats(const webrtc::RTCStats& stats) {
  return GetStatsWhitelist()->IsWhitelisted(stats);
}

// Filters stats that should be surfaced to JS. Stats are surfaced if they're
// standardized or if there is an active origin trial that enables a stat by
// including one of its group IDs in |exposed_group_ids|.
std::vector<const webrtc::RTCStatsMemberInterface*> FilterMembers(
    std::vector<const webrtc::RTCStatsMemberInterface*> stats_members,
    const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  // Note that using "is_standarized" avoids having to maintain a whitelist of
  // every single standardized member, as we do at the "stats object" level
  // with "RTCStatsWhitelist".
  base::EraseIf(
      stats_members,
      [&exposed_group_ids](const webrtc::RTCStatsMemberInterface* member) {
        if (member->is_standardized()) {
          return false;
        }

        const blink::WebVector<webrtc::NonStandardGroupId>& ids =
            member->group_ids();
        for (const webrtc::NonStandardGroupId& id : exposed_group_ids) {
          if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
            return false;
          }
        }
        return true;
      });
  return stats_members;
}

size_t CountWhitelistedStats(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report) {
  size_t size = 0;
  for (const auto& stats : *stats_report) {
    if (IsWhitelistedStats(stats)) {
      ++size;
    }
  }
  return size;
}

}  // namespace

RTCStatsReportPlatform::RTCStatsReportPlatform(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
    const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids)
    : stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()),
      exposed_group_ids_(exposed_group_ids),
      size_(CountWhitelistedStats(stats_report)) {
  DCHECK(stats_report_);
}

RTCStatsReportPlatform::~RTCStatsReportPlatform() {}

std::unique_ptr<RTCStatsReportPlatform> RTCStatsReportPlatform::CopyHandle()
    const {
  return std::make_unique<RTCStatsReportPlatform>(stats_report_,
                                                  exposed_group_ids_);
}

std::unique_ptr<RTCStats> RTCStatsReportPlatform::GetStats(
    const String& id) const {
  const webrtc::RTCStats* stats = stats_report_->Get(id.Utf8());
  if (!stats || !IsWhitelistedStats(*stats))
    return std::unique_ptr<RTCStats>();
  return std::make_unique<RTCStats>(stats_report_, stats, exposed_group_ids_);
}

std::unique_ptr<RTCStats> RTCStatsReportPlatform::Next() {
  while (it_ != end_) {
    const webrtc::RTCStats& next = *it_;
    ++it_;
    if (IsWhitelistedStats(next)) {
      return std::make_unique<RTCStats>(stats_report_, &next,
                                        exposed_group_ids_);
    }
  }
  return std::unique_ptr<RTCStats>();
}

size_t RTCStatsReportPlatform::Size() const {
  return size_;
}

RTCStats::RTCStats(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStats* stats,
    const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids)
    : stats_owner_(stats_owner),
      stats_(stats),
      stats_members_(FilterMembers(stats->Members(), exposed_group_ids)) {
  DCHECK(stats_owner_);
  DCHECK(stats_);
  DCHECK(stats_owner_->Get(stats_->id()));
}

RTCStats::~RTCStats() {}

String RTCStats::Id() const {
  return String::FromUTF8(stats_->id());
}

String RTCStats::GetType() const {
  return String::FromUTF8(stats_->type());
}

double RTCStats::Timestamp() const {
  return stats_->timestamp_us() /
         static_cast<double>(base::Time::kMicrosecondsPerMillisecond);
}

size_t RTCStats::MembersCount() const {
  return stats_members_.size();
}

std::unique_ptr<RTCStatsMember> RTCStats::GetMember(size_t i) const {
  DCHECK_LT(i, stats_members_.size());
  return std::make_unique<RTCStatsMember>(stats_owner_, stats_members_[i]);
}

RTCStatsMember::RTCStatsMember(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStatsMemberInterface* member)
    : stats_owner_(stats_owner), member_(member) {
  DCHECK(stats_owner_);
  DCHECK(member_);
}

RTCStatsMember::~RTCStatsMember() {}

String RTCStatsMember::GetName() const {
  return String::FromUTF8(member_->name());
}

webrtc::RTCStatsMemberInterface::Type RTCStatsMember::GetType() const {
  return member_->type();
}

bool RTCStatsMember::IsDefined() const {
  return member_->is_defined();
}

bool RTCStatsMember::ValueBool() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<bool>>();
}

int32_t RTCStatsMember::ValueInt32() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<int32_t>>();
}

uint32_t RTCStatsMember::ValueUint32() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<uint32_t>>();
}

int64_t RTCStatsMember::ValueInt64() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<int64_t>>();
}

uint64_t RTCStatsMember::ValueUint64() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<uint64_t>>();
}

double RTCStatsMember::ValueDouble() const {
  DCHECK(IsDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<double>>();
}

String RTCStatsMember::ValueString() const {
  DCHECK(IsDefined());
  return String::FromUTF8(
      *member_->cast_to<webrtc::RTCStatsMember<std::string>>());
}

blink::WebVector<int> RTCStatsMember::ValueSequenceBool() const {
  DCHECK(IsDefined());
  const std::vector<bool>& vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<bool>>>();
  std::vector<int> uint32_vector;
  uint32_vector.reserve(vector.size());
  for (size_t i = 0; i < vector.size(); ++i) {
    uint32_vector.push_back(vector[i] ? 1 : 0);
  }
  return blink::WebVector<int>(uint32_vector);
}

blink::WebVector<int32_t> RTCStatsMember::ValueSequenceInt32() const {
  DCHECK(IsDefined());
  return blink::WebVector<int32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int32_t>>>());
}

blink::WebVector<uint32_t> RTCStatsMember::ValueSequenceUint32() const {
  DCHECK(IsDefined());
  return blink::WebVector<uint32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint32_t>>>());
}

blink::WebVector<int64_t> RTCStatsMember::ValueSequenceInt64() const {
  DCHECK(IsDefined());
  return blink::WebVector<int64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int64_t>>>());
}

blink::WebVector<uint64_t> RTCStatsMember::ValueSequenceUint64() const {
  DCHECK(IsDefined());
  return blink::WebVector<uint64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint64_t>>>());
}

blink::WebVector<double> RTCStatsMember::ValueSequenceDouble() const {
  DCHECK(IsDefined());
  return blink::WebVector<double>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<double>>>());
}

blink::WebVector<String> RTCStatsMember::ValueSequenceString() const {
  DCHECK(IsDefined());
  const std::vector<std::string>& sequence =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<std::string>>>();
  blink::WebVector<String> web_sequence(sequence.size());
  for (size_t i = 0; i < sequence.size(); ++i)
    web_sequence[i] = String::FromUTF8(sequence[i]);
  return web_sequence;
}

rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    blink::WebRTCStatsReportCallback callback,
    const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  return rtc::scoped_refptr<RTCStatsCollectorCallbackImpl>(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackImpl>(
          std::move(main_thread), std::move(callback), exposed_group_ids));
}

RTCStatsCollectorCallbackImpl::RTCStatsCollectorCallbackImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    blink::WebRTCStatsReportCallback callback,
    const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids)
    : main_thread_(std::move(main_thread)),
      callback_(std::move(callback)),
      exposed_group_ids_(exposed_group_ids) {}

RTCStatsCollectorCallbackImpl::~RTCStatsCollectorCallbackImpl() {
  DCHECK(!callback_);
}

void RTCStatsCollectorCallbackImpl::OnStatsDelivered(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  PostCrossThreadTask(
      *main_thread_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCStatsCollectorCallbackImpl::OnStatsDeliveredOnMainThread,
          rtc::scoped_refptr<RTCStatsCollectorCallbackImpl>(this), report));
}

void RTCStatsCollectorCallbackImpl::OnStatsDeliveredOnMainThread(
    rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(report);
  DCHECK(callback_);
  // Make sure the callback is destroyed in the main thread as well.
  std::move(callback_).Run(std::make_unique<RTCStatsReportPlatform>(
      base::WrapRefCounted(report.get()), exposed_group_ids_));
}

void WhitelistStatsForTesting(const char* type) {
  GetStatsWhitelist()->WhitelistStatsForTesting(type);
}

}  // namespace blink
