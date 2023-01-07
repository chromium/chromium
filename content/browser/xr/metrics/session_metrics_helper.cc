// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/xr/metrics/session_metrics_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/xr/metrics/session_timer.h"
#include "content/browser/xr/metrics/webxr_session_tracker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/cpp/session_mode.h"

namespace content {

namespace {

const void* const kSessionMetricsHelperDataKey = &kSessionMetricsHelperDataKey;

// minimum duration: 7 seconds for video, no minimum for headset/vr modes
// maximum gap: 7 seconds between videos.  no gap for headset/vr-modes
constexpr base::TimeDelta kMinimumVideoSessionDuration(base::Seconds(7));
constexpr base::TimeDelta kMaximumVideoSessionGap(base::Seconds(7));

constexpr base::TimeDelta kMinimumHeadsetSessionDuration(base::Seconds(0));
constexpr base::TimeDelta kMaximumHeadsetSessionGap(base::Seconds(0));

// Handles the lifetime of the helper which is attached to a WebContents.
class SessionMetricsHelperData : public base::SupportsUserData::Data {
 public:
  SessionMetricsHelperData() = delete;

  explicit SessionMetricsHelperData(
      std::unique_ptr<SessionMetricsHelper> session_metrics_helper)
      : session_metrics_helper_(std::move(session_metrics_helper)) {}

  SessionMetricsHelperData(const SessionMetricsHelperData&) = delete;
  SessionMetricsHelperData& operator=(const SessionMetricsHelperData&) = delete;

  ~SessionMetricsHelperData() override = default;

  SessionMetricsHelper* get() const { return session_metrics_helper_.get(); }

 private:
  std::unique_ptr<SessionMetricsHelper> session_metrics_helper_;
};

// Helper method to log out both the mode and the initially requested features
// for a WebXRSessionTracker.  WebXRSessionTracker is an unowned pointer.
void ReportInitialSessionData(
    WebXRSessionTracker* webxr_session_tracker,
    const device::mojom::XRSessionOptions& session_options,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        enabled_features) {
  DCHECK(webxr_session_tracker);

  webxr_session_tracker->ukm_entry()->SetMode(
      static_cast<int64_t>(session_options.mode));
  webxr_session_tracker->ReportRequestedFeatures(session_options,
                                                 enabled_features);
}

}  // namespace

// static
SessionMetricsHelper* SessionMetricsHelper::FromWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents)
    return nullptr;
  SessionMetricsHelperData* data = static_cast<SessionMetricsHelperData*>(
      web_contents->GetUserData(kSessionMetricsHelperDataKey));
  return data ? data->get() : nullptr;
}

// static
SessionMetricsHelper* SessionMetricsHelper::CreateForWebContents(
    content::WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This is not leaked as the SessionMetricsHelperData will clean it up.
  std::unique_ptr<SessionMetricsHelper> helper =
      base::WrapUnique(new SessionMetricsHelper(contents));
  contents->SetUserData(
      kSessionMetricsHelperDataKey,
      std::make_unique<SessionMetricsHelperData>(std::move(helper)));
  return FromWebContents(contents);
}

SessionMetricsHelper::SessionMetricsHelper(content::WebContents* contents) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(contents);

  num_videos_playing_ = contents->GetCurrentlyPlayingVideoCount();

  Observe(contents);
}

SessionMetricsHelper::~SessionMetricsHelper() {
  DVLOG(2) << __func__;
}

mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
SessionMetricsHelper::StartInlineSession(
    const device::mojom::XRSessionOptions& session_options,
    const std::unordered_set<device::mojom::XRSessionFeature>& enabled_features,
    size_t session_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(webxr_inline_session_trackers_.find(session_id) ==
         webxr_inline_session_trackers_.end());

  // TODO(crbug.com/1061899): The code here assumes that it's called on
  // behalf of the active frame, which is not always true.
  // Plumb explicit RenderFrameHost reference from VRSessionImpl.
  auto result = webxr_inline_session_trackers_.emplace(
      session_id,
      std::make_unique<WebXRSessionTracker>(
          std::make_unique<ukm::builders::XR_WebXR_Session>(
              web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId())));
  auto* tracker = result.first->second.get();

  ReportInitialSessionData(tracker, session_options, enabled_features);

  return tracker->BindMetricsRecorderPipe();
}

void SessionMetricsHelper::StopAndRecordInlineSession(size_t session_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = webxr_inline_session_trackers_.find(session_id);

  if (it == webxr_inline_session_trackers_.end())
    return;

  it->second->SetSessionEnd(base::Time::Now());
  it->second->ukm_entry()->SetDuration(
      it->second->GetRoundedDurationInSeconds());
  it->second->RecordEntry();

  webxr_inline_session_trackers_.erase(it);
}

mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
SessionMetricsHelper::StartImmersiveSession(
    const device::mojom::XRSessionOptions& session_options,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        enabled_features) {
  DVLOG(1) << __func__;
  DCHECK(!webxr_immersive_session_tracker_);
  base::Time start_time = base::Time::Now();

  // TODO(crbug.com/1061899): The code here assumes that it's called on
  // behalf of the active frame, which is not always true.
  // Plumb explicit RenderFrameHost reference from VRSessionImpl.
  webxr_immersive_session_tracker_ = std::make_unique<WebXRSessionTracker>(
      std::make_unique<ukm::builders::XR_WebXR_Session>(
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));

  // TODO(https://crbug.com/1056930): Consider renaming the timers to something
  // that indicates both that these also record AR, and that these are no longer
  // "suffixed" histograms.
  session_timer_ = std::make_unique<SessionTimer>(
      "VRSessionTime.WebVR", kMaximumHeadsetSessionGap,
      kMinimumHeadsetSessionDuration);
  session_timer_->StartSession(start_time);

  session_video_timer_ = std::make_unique<SessionTimer>(
      "VRSessionVideoTime.WebVR", kMaximumVideoSessionGap,
      kMinimumVideoSessionDuration);

  num_session_video_playback_ = num_videos_playing_;

  if (num_videos_playing_ > 0) {
    session_video_timer_->StartSession(start_time);
  }

  ReportInitialSessionData(webxr_immersive_session_tracker_.get(),
                           session_options, enabled_features);

  return webxr_immersive_session_tracker_->BindMetricsRecorderPipe();
}

void SessionMetricsHelper::StopAndRecordImmersiveSession() {
  DVLOG(1) << __func__;
  // A session cannot outlive a navigation, so we terminate it here. However,
  // depending on how the session is torn down, we may be notified in any order
  // of the navigation and then shutdown. If we don't have an active session,
  // assume it's been stopped already and just return early.
  if (!webxr_immersive_session_tracker_) {
    return;
  }

  webxr_immersive_session_tracker_->SetSessionEnd(base::Time::Now());
  webxr_immersive_session_tracker_->ukm_entry()->SetDuration(
      webxr_immersive_session_tracker_->GetRoundedDurationInSeconds());
  webxr_immersive_session_tracker_->RecordEntry();
  webxr_immersive_session_tracker_ = nullptr;

  // Destroyig the timers will both stop the session and force them to log their
  // metrics.
  session_timer_ = nullptr;
  session_video_timer_ = nullptr;

  UMA_HISTOGRAM_COUNTS_100("VRSessionVideoCount", num_session_video_playback_);
}

void SessionMetricsHelper::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  if (num_videos_playing_ == 0) {
    // started playing video - start sessions
    base::Time start_time = base::Time::Now();

    if (session_video_timer_) {
      session_video_timer_->StartSession(start_time);
    }
  }

  num_videos_playing_++;
  num_session_video_playback_++;
}

void SessionMetricsHelper::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId&,
    WebContentsObserver::MediaStoppedReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  num_videos_playing_--;

  if (num_videos_playing_ == 0) {
    // stopped playing video - update existing video sessions
    base::Time stop_time = base::Time::Now();

    if (session_video_timer_) {
      session_video_timer_->StopSession(true, stop_time);
    }
  }
}

void SessionMetricsHelper::PrimaryPageChanged(content::Page& page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // All sessions are terminated on navigations, so to ensure that we log
  // everything that we have, cleanup any outstanding session trackers now.
  if (webxr_immersive_session_tracker_)
    StopAndRecordImmersiveSession();

  for (auto& inline_session_tracker : webxr_inline_session_trackers_) {
    inline_session_tracker.second->SetSessionEnd(base::Time::Now());
    inline_session_tracker.second->ukm_entry()->SetDuration(
        inline_session_tracker.second->GetRoundedDurationInSeconds());
    inline_session_tracker.second->RecordEntry();
  }

  webxr_inline_session_trackers_.clear();
}

}  // namespace content
