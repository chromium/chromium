// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>

#include "base/check_op.h"
#include "base/containers/contains.h"
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

// TODO(https://crbug.com/webrtc/14175): When "track" stats no longer exist in
// the lower layer, delete all the filtering mechanisms gated by this flag since
// that filtering will become a NO-OP when "track" no longer exists.
BASE_FEATURE(WebRtcUnshipDeprecatedStats,
             "WebRtcUnshipDeprecatedStats",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

bool MemberIsReferenceToDeprecated(
    const webrtc::RTCStatsMemberInterface* member) {
  // ID references are strings with a defined value.
  if (member->type() != webrtc::RTCStatsMemberInterface::Type::kString ||
      !member->is_defined()) {
    return false;
  }
  const char* member_name = member->name();
  size_t len = strlen(member_name);
  // ID referenced end with "Id" by naming convention.
  if (len < 2 || member_name[len - 2] != 'I' || member_name[len - 1] != 'd')
    return false;
  const std::string& id_reference =
      *member->cast_to<webrtc::RTCStatsMember<std::string>>();
  // starts_with()
  return id_reference.rfind("DEPRECATED_", 0) == 0;
}

// Members are surfaced if one of the following is true:
// - They're standardized and if `unship_deprecated_stats` is true they aren't
//   references to a deprecated object.
// - There is an active origin trial exposing that particular member.
// - There is an active feature exposing non-standard stats.
std::vector<const webrtc::RTCStatsMemberInterface*> FilterMembers(
    std::vector<const webrtc::RTCStatsMemberInterface*> stats_members,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
    bool unship_deprecated_stats) {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebRtcExposeNonStandardStats)) {
    return stats_members;
  }
  base::EraseIf(
      stats_members, [&exposed_group_ids, &unship_deprecated_stats](
                         const webrtc::RTCStatsMemberInterface* member) {
        if (member->is_standardized()) {
          // Standard members are only erased when filtering out "DEPRECATED_"
          // ID references.
          return unship_deprecated_stats &&
                 MemberIsReferenceToDeprecated(member);
        }
        // Non-standard members are erased unless part of the exposed groups.
        const std::vector<webrtc::NonStandardGroupId>& ids =
            member->group_ids();
        for (const webrtc::NonStandardGroupId& id : exposed_group_ids) {
          if (base::Contains(ids, id)) {
            return false;
          }
        }
        return true;
      });
  return stats_members;
}

template <typename T>
Vector<T> ToWTFVector(const std::vector<T>& vector) {
  Vector<T> wtf_vector(base::checked_cast<WTF::wtf_size_t>(vector.size()));
  std::move(vector.begin(), vector.end(), wtf_vector.begin());
  return wtf_vector;
}

bool ShouldExposeStatsObject(const webrtc::RTCStats& stats,
                             bool unship_deprecated_stats) {
  if (!unship_deprecated_stats)
    return true;
  // !starts_with()
  return stats.id().rfind("DEPRECATED_", 0) != 0;
}

size_t CountExposedStatsObjects(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
    bool unship_deprecated_stats) {
  if (!unship_deprecated_stats)
    return stats_report->size();
  size_t count = 0u;
  for (const auto& stats : *stats_report) {
    if (ShouldExposeStatsObject(stats, unship_deprecated_stats))
      ++count;
  }
  return count;
}

}  // namespace

RTCStatsReportPlatform::RTCStatsReportPlatform(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
    bool is_track_stats_deprecation_trial_enabled)
    : is_track_stats_deprecation_trial_enabled_(
          is_track_stats_deprecation_trial_enabled),
      unship_deprecated_stats_(
          base::FeatureList::IsEnabled(WebRtcUnshipDeprecatedStats) &&
          !is_track_stats_deprecation_trial_enabled_),
      stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()),
      exposed_group_ids_(exposed_group_ids),
      size_(CountExposedStatsObjects(stats_report, unship_deprecated_stats_)) {
  DCHECK(stats_report_);
}

RTCStatsReportPlatform::~RTCStatsReportPlatform() {}

std::unique_ptr<RTCStatsReportPlatform> RTCStatsReportPlatform::CopyHandle()
    const {
  return std::make_unique<RTCStatsReportPlatform>(
      stats_report_, exposed_group_ids_,
      is_track_stats_deprecation_trial_enabled_);
}

std::unique_ptr<RTCStatsWrapper> RTCStatsReportPlatform::GetStats(
    const String& id) const {
  const webrtc::RTCStats* stats = stats_report_->Get(id.Utf8());
  if (!stats || !ShouldExposeStatsObject(*stats, unship_deprecated_stats_))
    return std::unique_ptr<RTCStatsWrapper>();
  return std::make_unique<RTCStatsWrapper>(
      stats_report_, stats, exposed_group_ids_, unship_deprecated_stats_);
}

std::unique_ptr<RTCStatsWrapper> RTCStatsReportPlatform::Next() {
  while (it_ != end_) {
    const webrtc::RTCStats& next = *it_;
    ++it_;
    if (ShouldExposeStatsObject(next, unship_deprecated_stats_)) {
      return std::make_unique<RTCStatsWrapper>(
          stats_report_, &next, exposed_group_ids_, unship_deprecated_stats_);
    }
  }
  return std::unique_ptr<RTCStatsWrapper>();
}

const webrtc::RTCStats* RTCStatsReportPlatform::NextStats() {
  while (it_ != end_) {
    const webrtc::RTCStats& stat = *it_;
    ++it_;
    return &stat;
  }
  return nullptr;
}

size_t RTCStatsReportPlatform::Size() const {
  return size_;
}

RTCStatsWrapper::RTCStatsWrapper(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStats* stats,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
    bool unship_deprecated_stats)
    : stats_owner_(stats_owner),
      stats_(stats),
      stats_members_(FilterMembers(stats->Members(),
                                   exposed_group_ids,
                                   unship_deprecated_stats)) {
  DCHECK(stats_owner_);
  DCHECK(stats_);
  DCHECK(stats_owner_->Get(stats_->id()));
}

RTCStatsWrapper::~RTCStatsWrapper() = default;

String RTCStatsWrapper::Id() const {
  return String::FromUTF8(stats_->id());
}

String RTCStatsWrapper::GetType() const {
  return String::FromUTF8(stats_->type());
}

double RTCStatsWrapper::TimestampMs() const {
  // The timestamp unit is milliseconds but we want decimal
  // precision so we convert ourselves.
  return stats_->timestamp().us() /
         static_cast<double>(base::Time::kMicrosecondsPerMillisecond);
}

size_t RTCStatsWrapper::MembersCount() const {
  return stats_members_.size();
}

std::unique_ptr<RTCStatsMember> RTCStatsWrapper::GetMember(size_t i) const {
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

RTCStatsMember::~RTCStatsMember() = default;

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

RTCStatsMember::ExposureRestriction RTCStatsMember::Restriction() const {
  switch (member_->exposure_criteria()) {
    case webrtc::StatExposureCriteria::kHardwareCapability:
      return ExposureRestriction::kHardwareCapability;
    case webrtc::StatExposureCriteria::kAlways:
    default:
      return ExposureRestriction::kNone;
  }
}

rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
    bool is_track_stats_deprecation_trial_enabled) {
  return rtc::scoped_refptr<RTCStatsCollectorCallbackImpl>(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackImpl>(
          std::move(main_thread), std::move(callback), exposed_group_ids,
          is_track_stats_deprecation_trial_enabled));
}

RTCStatsCollectorCallbackImpl::RTCStatsCollectorCallbackImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids,
    bool is_track_stats_deprecation_trial_enabled)
    : main_thread_(std::move(main_thread)),
      callback_(std::move(callback)),
      exposed_group_ids_(exposed_group_ids),
      is_track_stats_deprecation_trial_enabled_(
          is_track_stats_deprecation_trial_enabled) {}

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
      base::WrapRefCounted(report.get()), exposed_group_ids_,
      is_track_stats_deprecation_trial_enabled_));
}

}  // namespace blink
