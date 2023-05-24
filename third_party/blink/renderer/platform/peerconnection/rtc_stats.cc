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
    bool is_track_stats_deprecation_trial_enabled)
    : is_track_stats_deprecation_trial_enabled_(
          is_track_stats_deprecation_trial_enabled),
      unship_deprecated_stats_(
          base::FeatureList::IsEnabled(WebRtcUnshipDeprecatedStats) &&
          !is_track_stats_deprecation_trial_enabled_),
      stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()),
      size_(CountExposedStatsObjects(stats_report, unship_deprecated_stats_)) {
  DCHECK(stats_report_);
}

RTCStatsReportPlatform::~RTCStatsReportPlatform() {}

std::unique_ptr<RTCStatsReportPlatform> RTCStatsReportPlatform::CopyHandle()
    const {
  return std::make_unique<RTCStatsReportPlatform>(
      stats_report_, is_track_stats_deprecation_trial_enabled_);
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

rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    bool is_track_stats_deprecation_trial_enabled) {
  return rtc::scoped_refptr<RTCStatsCollectorCallbackImpl>(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackImpl>(
          std::move(main_thread), std::move(callback),
          is_track_stats_deprecation_trial_enabled));
}

RTCStatsCollectorCallbackImpl::RTCStatsCollectorCallbackImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCStatsReportCallback callback,
    bool is_track_stats_deprecation_trial_enabled)
    : main_thread_(std::move(main_thread)),
      callback_(std::move(callback)),
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
      base::WrapRefCounted(report.get()),
      is_track_stats_deprecation_trial_enabled_));
}

}  // namespace blink
