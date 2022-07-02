// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/check_op.h"
#include "base/containers/cxx20_erase.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

namespace blink {

namespace {

class RTCStatsAllowlist {
 public:
  RTCStatsAllowlist() {
    allowlisted_stats_types_.insert(webrtc::RTCCertificateStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCCodecStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCDataChannelStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCIceCandidatePairStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCIceCandidateStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCLocalIceCandidateStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCRemoteIceCandidateStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCMediaStreamStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCMediaStreamTrackStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCPeerConnectionStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCRTPStreamStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCInboundRTPStreamStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCOutboundRTPStreamStats::kType);
    allowlisted_stats_types_.insert(
        webrtc::RTCRemoteInboundRtpStreamStats::kType);
    allowlisted_stats_types_.insert(
        webrtc::RTCRemoteOutboundRtpStreamStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCMediaSourceStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCAudioSourceStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCVideoSourceStats::kType);
    allowlisted_stats_types_.insert(webrtc::RTCTransportStats::kType);
  }

  bool IsAllowlisted(const webrtc::RTCStats& stats) {
    return allowlisted_stats_types_.find(stats.type()) !=
           allowlisted_stats_types_.end();
  }

  void AllowStatsForTesting(const char* type) {
    allowlisted_stats_types_.insert(type);
  }

 private:
  std::set<std::string> allowlisted_stats_types_;
};

RTCStatsAllowlist* GetStatsAllowlist() {
  static RTCStatsAllowlist* list = new RTCStatsAllowlist();
  return list;
}

bool IsAllowlistedStats(const webrtc::RTCStats& stats) {
  return GetStatsAllowlist()->IsAllowlisted(stats);
}

// Filters stats that should be surfaced to JS. Stats are surfaced if they're
// standardized or if there is an active origin trial that enables a stat by
// including one of its group IDs in |exposed_group_ids|.
std::vector<const webrtc::RTCStatsMemberInterface*> FilterMembers(
    std::vector<const webrtc::RTCStatsMemberInterface*> stats_members,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebRtcExposeNonStandardStats)) {
    return stats_members;
  }
  // Note that using "is_standarized" avoids having to maintain an allowlist of
  // every single standardized member, as we do at the "stats object" level
  // with "RTCStatsAllowlist".
  base::EraseIf(
      stats_members,
      [&exposed_group_ids](const webrtc::RTCStatsMemberInterface* member) {
        if (member->is_standardized()) {
          return false;
        }

        const std::vector<webrtc::NonStandardGroupId>& ids =
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

size_t CountAllowlistedStats(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report) {
  size_t size = 0;
  for (const auto& stats : *stats_report) {
    if (IsAllowlistedStats(stats)) {
      ++size;
    }
  }
  return size;
}

template <typename T>
Vector<T> ToWTFVector(const std::vector<T>& vector) {
  Vector<T> wtf_vector(base::checked_cast<WTF::wtf_size_t>(vector.size()));
  std::move(vector.begin(), vector.end(), wtf_vector.begin());
  return wtf_vector;
}

}  // namespace

RTCStatsReportPlatform::RTCStatsReportPlatform(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids)
    : stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()),
      exposed_group_ids_(exposed_group_ids),
      size_(CountAllowlistedStats(stats_report)) {
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
  if (!stats || !IsAllowlistedStats(*stats))
    return std::unique_ptr<RTCStats>();
  return std::make_unique<RTCStats>(stats_report_, stats, exposed_group_ids_);
}

std::unique_ptr<RTCStats> RTCStatsReportPlatform::Next() {
  while (it_ != end_) {
    const webrtc::RTCStats& next = *it_;
    ++it_;
    if (IsAllowlistedStats(next)) {
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
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids)
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

Vector<bool> RTCStatsMember::ValueSequenceBool() const {
  DCHECK(IsDefined());
  const std::vector<bool> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<bool>>>();
  return ToWTFVector(vector);
}

Vector<int32_t> RTCStatsMember::ValueSequenceInt32() const {
  DCHECK(IsDefined());
  const std::vector<int32_t> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int32_t>>>();
  return ToWTFVector(vector);
}

Vector<uint32_t> RTCStatsMember::ValueSequenceUint32() const {
  DCHECK(IsDefined());
  const std::vector<uint32_t> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint32_t>>>();
  return ToWTFVector(vector);
}

Vector<int64_t> RTCStatsMember::ValueSequenceInt64() const {
  DCHECK(IsDefined());
  const std::vector<int64_t> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int64_t>>>();
  return ToWTFVector(vector);
}

Vector<uint64_t> RTCStatsMember::ValueSequenceUint64() const {
  DCHECK(IsDefined());
  const std::vector<uint64_t> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint64_t>>>();
  return ToWTFVector(vector);
}

Vector<double> RTCStatsMember::ValueSequenceDouble() const {
  DCHECK(IsDefined());
  const std::vector<double> vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<double>>>();
  return ToWTFVector(vector);
}

Vector<String> RTCStatsMember::ValueSequenceString() const {
  DCHECK(IsDefined());
  const std::vector<std::string>& sequence =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<std::string>>>();
  Vector<String> wtf_sequence(base::checked_cast<wtf_size_t>(sequence.size()));
  for (wtf_size_t i = 0; i < wtf_sequence.size(); ++i)
    wtf_sequence[i] = String::FromUTF8(sequence[i]);
  return wtf_sequence;
}

HashMap<String, uint64_t> RTCStatsMember::ValueMapStringUint64() const {
  DCHECK(IsDefined());
  const std::map<std::string, uint64_t>& map =
      *member_
           ->cast_to<webrtc::RTCStatsMember<std::map<std::string, uint64_t>>>();
  HashMap<String, uint64_t> wtf_map;
  wtf_map.ReserveCapacityForSize(base::checked_cast<unsigned>(map.size()));
  for (auto& elem : map) {
    wtf_map.insert(String::FromUTF8(elem.first), elem.second);
  }
  return wtf_map;
}

HashMap<String, double> RTCStatsMember::ValueMapStringDouble() const {
  DCHECK(IsDefined());
  const std::map<std::string, double>& map =
      *member_
           ->cast_to<webrtc::RTCStatsMember<std::map<std::string, double>>>();
  HashMap<String, double> wtf_map;
  wtf_map.ReserveCapacityForSize(base::checked_cast<unsigned>(map.size()));
  for (auto& elem : map) {
    wtf_map.insert(String::FromUTF8(elem.first), elem.second);
  }
  return wtf_map;
}

rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  return rtc::scoped_refptr<RTCStatsCollectorCallbackImpl>(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackImpl>(
          std::move(main_thread), std::move(callback), exposed_group_ids));
}

RTCStatsCollectorCallbackImpl::RTCStatsCollectorCallbackImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids)
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

void AllowStatsForTesting(const char* type) {
  GetStatsAllowlist()->AllowStatsForTesting(type);
}

}  // namespace blink
