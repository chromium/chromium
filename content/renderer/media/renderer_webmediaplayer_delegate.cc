// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmediaplayer_delegate.h"

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/sys_info.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {

void RecordAction(const base::UserMetricsAction& action) {
  content::RenderThread::Get()->RecordAction(action);
}

}  // namespace

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      allow_idle_cleanup_(
          content::GetContentClient()->renderer()->IsIdleMediaSuspendEnabled()),
      background_suspend_enabled_(
          content::GetContentClient()
              ->renderer()
              ->IsBackgroundMediaSuspendEnabled(render_frame)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  idle_cleanup_interval_ = base::TimeDelta::FromSeconds(5);
  idle_timeout_ = base::TimeDelta::FromSeconds(15);

  is_jelly_bean_ = false;

#if defined(OS_ANDROID)
  // On Android, due to the instability of the OS level media components, we
  // consider all pre-KitKat devices to be potentially buggy.
  is_jelly_bean_ |= base::android::BuildInfo::GetInstance()->sdk_int() <=
                    base::android::SDK_VERSION_JELLY_BEAN_MR2;
#endif

  idle_cleanup_timer_.SetTaskRunner(
      render_frame->GetTaskRunner(blink::TaskType::kInternalMedia));
}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

bool RendererWebMediaPlayerDelegate::IsFrameHidden() {
  if (is_frame_hidden_for_testing_)
    return true;

  return (render_frame() && render_frame()->IsHidden()) || is_frame_closed_;
}

bool RendererWebMediaPlayerDelegate::IsFrameClosed() {
  return is_frame_closed_;
}

int RendererWebMediaPlayerDelegate::AddObserver(Observer* observer) {
  return id_map_.Add(observer);
}

void RendererWebMediaPlayerDelegate::RemoveObserver(int player_id) {
  DCHECK(id_map_.Lookup(player_id));
  id_map_.Remove(player_id);
  idle_player_map_.erase(player_id);
  stale_players_.erase(player_id);
  playing_videos_.erase(player_id);

  Send(
      new MediaPlayerDelegateHostMsg_OnMediaDestroyed(routing_id(), player_id));

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::DidPlay(
    int player_id,
    bool has_video,
    bool has_audio,
    MediaContentType media_content_type) {
  DVLOG(2) << __func__ << "(" << player_id << ", " << has_video << ", "
           << has_audio << ", " << static_cast<int>(media_content_type) << ")";
  DCHECK(id_map_.Lookup(player_id));

  has_played_media_ = true;
  if (has_video) {
    if (!playing_videos_.count(player_id)) {
      playing_videos_.insert(player_id);
      has_played_video_ = true;
    }
  } else {
    playing_videos_.erase(player_id);
  }

  Send(new MediaPlayerDelegateHostMsg_OnMediaPlaying(
      routing_id(), player_id, has_video, has_audio, false,
      media_content_type));

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::DidPlayerMutedStatusChange(int delegate_id,
                                                                bool muted) {
  Send(new MediaPlayerDelegateHostMsg_OnMutedStatusChanged(routing_id(),
                                                           delegate_id, muted));
}

void RendererWebMediaPlayerDelegate::DidPictureInPictureModeStart(
    int delegate_id,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    blink::WebMediaPlayer::PipWindowOpenedCallback callback,
    bool show_play_pause_button) {
  int request_id = next_picture_in_picture_callback_id_++;
  enter_picture_in_picture_callback_map_.insert(
      std::make_pair(request_id, std::move(callback)));
  Send(new MediaPlayerDelegateHostMsg_OnPictureInPictureModeStarted(
      routing_id(), delegate_id, surface_id, natural_size, request_id,
      show_play_pause_button));
}

void RendererWebMediaPlayerDelegate::DidPictureInPictureModeEnd(
    int delegate_id,
    base::OnceClosure callback) {
  int request_id = next_picture_in_picture_callback_id_++;
  exit_picture_in_picture_callback_map_.insert(
      std::make_pair(request_id, std::move(callback)));
  Send(new MediaPlayerDelegateHostMsg_OnPictureInPictureModeEnded(
      routing_id(), delegate_id, request_id));
}

void RendererWebMediaPlayerDelegate::DidSetPictureInPictureCustomControls(
    int delegate_id,
    const std::vector<blink::PictureInPictureControlInfo>& controls) {
  Send(new MediaPlayerDelegateHostMsg_OnSetPictureInPictureCustomControls(
      routing_id(), delegate_id, controls));
}

void RendererWebMediaPlayerDelegate::DidPictureInPictureSurfaceChange(
    int delegate_id,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button) {
  Send(new MediaPlayerDelegateHostMsg_OnPictureInPictureSurfaceChanged(
      routing_id(), delegate_id, surface_id, natural_size,
      show_play_pause_button));
}

void RendererWebMediaPlayerDelegate::
    RegisterPictureInPictureWindowResizeCallback(
        int player_id,
        blink::WebMediaPlayer::PipWindowResizedCallback callback) {
  picture_in_picture_window_resize_observer_ =
      std::make_pair(player_id, std::move(callback));
}

bool RendererWebMediaPlayerDelegate::IsBackgroundMediaSuspendEnabled() {
  return background_suspend_enabled_;
}

void RendererWebMediaPlayerDelegate::DidPause(int player_id) {
  DVLOG(2) << __func__ << "(" << player_id << ")";
  DCHECK(id_map_.Lookup(player_id));
  playing_videos_.erase(player_id);
  Send(new MediaPlayerDelegateHostMsg_OnMediaPaused(routing_id(), player_id,
                                                    false));

  // Required to keep background playback statistics up to date.
  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::PlayerGone(int player_id) {
  DVLOG(2) << __func__ << "(" << player_id << ")";
  DCHECK(id_map_.Lookup(player_id));
  playing_videos_.erase(player_id);
  Send(
      new MediaPlayerDelegateHostMsg_OnMediaDestroyed(routing_id(), player_id));

  // Required to keep background playback statistics up to date.
  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::SetIdle(int player_id, bool is_idle) {
  DVLOG(2) << __func__ << "(" << player_id << ", " << is_idle << ")";

  if (is_idle == IsIdle(player_id))
    return;

  if (is_idle) {
    idle_player_map_[player_id] = tick_clock_->NowTicks();
  } else {
    idle_player_map_.erase(player_id);
    stale_players_.erase(player_id);
  }

  ScheduleUpdateTask();
}

bool RendererWebMediaPlayerDelegate::IsIdle(int player_id) {
  return idle_player_map_.count(player_id) || stale_players_.count(player_id);
}

void RendererWebMediaPlayerDelegate::ClearStaleFlag(int player_id) {
  DVLOG(2) << __func__ << "(" << player_id << ")";

  if (!stale_players_.erase(player_id))
    return;

  // Set the idle time such that the player will be considered stale the next
  // time idle cleanup runs.
  idle_player_map_[player_id] = tick_clock_->NowTicks() - idle_timeout_;

  // No need to call Update immediately, just make sure the idle
  // timer is running. Calling ScheduleUpdateTask() here will cause
  // immediate cleanup, and if that fails, this function gets called
  // again which uses 100% cpu until resolved.
  if (!idle_cleanup_timer_.IsRunning() && !pending_update_task_) {
    idle_cleanup_timer_.Start(
        FROM_HERE, idle_cleanup_interval_,
        base::BindOnce(&RendererWebMediaPlayerDelegate::UpdateTask,
                       base::Unretained(this)));
  }
}

bool RendererWebMediaPlayerDelegate::IsStale(int player_id) {
  return stale_players_.count(player_id);
}

void RendererWebMediaPlayerDelegate::SetIsEffectivelyFullscreen(
    int player_id,
    blink::WebFullscreenVideoStatus fullscreen_video_status) {
  Send(new MediaPlayerDelegateHostMsg_OnMediaEffectivelyFullscreenChanged(
      routing_id(), player_id, fullscreen_video_status));
}

void RendererWebMediaPlayerDelegate::DidPlayerSizeChange(
    int delegate_id,
    const gfx::Size& size) {
  Send(new MediaPlayerDelegateHostMsg_OnMediaSizeChanged(routing_id(),
                                                         delegate_id, size));
}

void RendererWebMediaPlayerDelegate::WasHidden() {
  RecordAction(base::UserMetricsAction("Media.Hidden"));

  for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
       it.Advance())
    it.GetCurrentValue()->OnFrameHidden();

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::WasShown() {
  RecordAction(base::UserMetricsAction("Media.Shown"));
  is_frame_closed_ = false;

  for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
       it.Advance())
    it.GetCurrentValue()->OnFrameShown();

  ScheduleUpdateTask();
}

bool RendererWebMediaPlayerDelegate::OnMessageReceived(
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(RendererWebMediaPlayerDelegate, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Pause, OnMediaDelegatePause)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Play, OnMediaDelegatePlay)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_SeekForward,
                        OnMediaDelegateSeekForward)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_SeekBackward,
                        OnMediaDelegateSeekBackward)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_SuspendAllMediaPlayers,
                        OnMediaDelegateSuspendAllMediaPlayers)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_UpdateVolumeMultiplier,
                        OnMediaDelegateVolumeMultiplierUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_BecamePersistentVideo,
                        OnMediaDelegateBecamePersistentVideo)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_EndPictureInPictureMode,
                        OnPictureInPictureModeEnded)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_ClickPictureInPictureControl,
                        OnPictureInPictureControlClicked)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_OnPictureInPictureModeEnded_ACK,
                        OnPictureInPictureModeEndedAck)
    IPC_MESSAGE_HANDLER(
        MediaPlayerDelegateMsg_OnPictureInPictureModeStarted_ACK,
        OnPictureInPictureModeStartedAck)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_OnPictureInPictureWindowResize,
                        OnPictureInPictureWindowResize)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void RendererWebMediaPlayerDelegate::SetIdleCleanupParamsForTesting(
    base::TimeDelta idle_timeout,
    base::TimeDelta idle_cleanup_interval,
    const base::TickClock* tick_clock,
    bool is_jelly_bean) {
  idle_cleanup_interval_ = idle_cleanup_interval;
  idle_timeout_ = idle_timeout;
  tick_clock_ = tick_clock;
  is_jelly_bean_ = is_jelly_bean;
}

bool RendererWebMediaPlayerDelegate::IsIdleCleanupTimerRunningForTesting()
    const {
  return idle_cleanup_timer_.IsRunning();
}

void RendererWebMediaPlayerDelegate::SetFrameHiddenForTesting(bool is_hidden) {
  if (is_hidden == is_frame_hidden_for_testing_)
    return;

  is_frame_hidden_for_testing_ = is_hidden;

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePause(int player_id) {
  RecordAction(base::UserMetricsAction("Media.Controls.RemotePause"));

  Observer* observer = id_map_.Lookup(player_id);
  if (observer) {
    // TODO(avayvod): remove when default play/pause is handled via
    // the MediaSession code path.
    std::unique_ptr<blink::WebScopedUserGesture> gesture(
        render_frame()
            ? new blink::WebScopedUserGesture(render_frame()->GetWebFrame())
            : nullptr);
    observer->OnPause();
  }
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePlay(int player_id) {
  RecordAction(base::UserMetricsAction("Media.Controls.RemotePlay"));

  Observer* observer = id_map_.Lookup(player_id);
  if (observer) {
    // TODO(avayvod): remove when default play/pause is handled via
    // the MediaSession code path.
    std::unique_ptr<blink::WebScopedUserGesture> gesture(
        render_frame()
            ? new blink::WebScopedUserGesture(render_frame()->GetWebFrame())
            : nullptr);
    observer->OnPlay();
  }
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateSeekForward(
    int player_id,
    base::TimeDelta seek_time) {
  RecordAction(base::UserMetricsAction("Media.Controls.RemoteSeekForward"));

  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnSeekForward(seek_time.InSecondsF());
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateSeekBackward(
    int player_id,
    base::TimeDelta seek_time) {
  RecordAction(base::UserMetricsAction("Media.Controls.RemoteSeekBackward"));

  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnSeekBackward(seek_time.InSecondsF());
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateSuspendAllMediaPlayers() {
  is_frame_closed_ = true;

  for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
       it.Advance())
    it.GetCurrentValue()->OnFrameClosed();
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateVolumeMultiplierUpdate(
    int player_id,
    double multiplier) {
  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnVolumeMultiplierUpdate(multiplier);
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateBecamePersistentVideo(
    int player_id,
    bool value) {
  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnBecamePersistentVideo(value);
}

void RendererWebMediaPlayerDelegate::OnPictureInPictureModeEnded(
    int player_id) {
  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnPictureInPictureModeEnded();
}

void RendererWebMediaPlayerDelegate::OnPictureInPictureControlClicked(
    int player_id,
    const std::string& control_id) {
  Observer* observer = id_map_.Lookup(player_id);
  if (observer)
    observer->OnPictureInPictureControlClicked(control_id);
}

void RendererWebMediaPlayerDelegate::OnPictureInPictureModeEndedAck(
    int player_id,
    int request_id) {
  auto iter = exit_picture_in_picture_callback_map_.find(request_id);
  DCHECK(iter != exit_picture_in_picture_callback_map_.end());

  std::move(iter->second).Run();
  exit_picture_in_picture_callback_map_.erase(iter);
}

void RendererWebMediaPlayerDelegate::OnPictureInPictureModeStartedAck(
    int player_id,
    int request_id,
    const gfx::Size& window_size) {
  auto iter = enter_picture_in_picture_callback_map_.find(request_id);
  DCHECK(iter != enter_picture_in_picture_callback_map_.end());

  std::move(iter->second).Run(blink::WebSize(window_size));
  enter_picture_in_picture_callback_map_.erase(iter);
}

void RendererWebMediaPlayerDelegate::OnPictureInPictureWindowResize(
    int player_id,
    const gfx::Size& window_size) {
  if (!picture_in_picture_window_resize_observer_ ||
      picture_in_picture_window_resize_observer_->first != player_id) {
    return;
  }

  picture_in_picture_window_resize_observer_->second.Run(
      blink::WebSize(window_size));
}

void RendererWebMediaPlayerDelegate::ScheduleUpdateTask() {
  if (!pending_update_task_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&RendererWebMediaPlayerDelegate::UpdateTask,
                                  AsWeakPtr()));
    pending_update_task_ = true;
  }
}

void RendererWebMediaPlayerDelegate::UpdateTask() {
  DVLOG(3) << __func__;
  pending_update_task_ = false;

  // Check whether a player was played since the last UpdateTask(). We basically
  // treat this as a parameter to UpdateTask(), except that it can be changed
  // between posting the task and UpdateTask() executing.
  bool has_played_video_since_last_update_task = has_played_video_;
  has_played_video_ = false;

  // Record UMAs for background video playback.
  RecordBackgroundVideoPlayback();

  if (!allow_idle_cleanup_)
    return;

  // Clean up idle players.
  bool aggressive_cleanup = false;

  // When we reach the maximum number of idle players, clean them up
  // aggressively. Values chosen after testing on a Galaxy Nexus device for
  // http://crbug.com/612909.
  if (idle_player_map_.size() > (is_jelly_bean_ ? 2u : 8u))
    aggressive_cleanup = true;

  // When a player plays on a buggy old device, clean up idle players
  // aggressively.
  if (has_played_video_since_last_update_task && is_jelly_bean_)
    aggressive_cleanup = true;

  CleanUpIdlePlayers(aggressive_cleanup ? base::TimeDelta() : idle_timeout_);

  // If there are still idle players, schedule an attempt to clean them up.
  // This construct ensures that the next callback is always
  // |idle_cleanup_interval_| from now.
  idle_cleanup_timer_.Stop();
  if (!idle_player_map_.empty()) {
    idle_cleanup_timer_.Start(
        FROM_HERE, idle_cleanup_interval_,
        base::BindOnce(&RendererWebMediaPlayerDelegate::UpdateTask,
                       base::Unretained(this)));
  }
}

void RendererWebMediaPlayerDelegate::RecordBackgroundVideoPlayback() {
#if defined(OS_ANDROID)
  // TODO(avayvod): This would be useful to collect on desktop too and express
  // in actual media watch time vs. just elapsed time.
  // See https://crbug.com/638726.
  bool has_playing_background_video =
      IsFrameHidden() && !IsFrameClosed() && !playing_videos_.empty();

  if (has_playing_background_video != was_playing_background_video_) {
    was_playing_background_video_ = has_playing_background_video;

    if (has_playing_background_video) {
      RecordAction(base::UserMetricsAction("Media.Session.BackgroundResume"));
      background_video_start_time_ = base::TimeTicks::Now();
    } else {
      RecordAction(base::UserMetricsAction("Media.Session.BackgroundSuspend"));
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Media.Android.BackgroundVideoTime",
          base::TimeTicks::Now() - background_video_start_time_,
          base::TimeDelta::FromSeconds(7), base::TimeDelta::FromHours(10), 50);
    }
  }
#endif  // OS_ANDROID
}

void RendererWebMediaPlayerDelegate::CleanUpIdlePlayers(
    base::TimeDelta timeout) {
  const base::TimeTicks now = tick_clock_->NowTicks();

  // Create a list of stale players before making any possibly reentrant calls
  // to OnIdleTimeout().
  std::vector<int> stale_players;
  for (const auto& it : idle_player_map_) {
    if (now - it.second >= timeout)
      stale_players.push_back(it.first);
  }

  // Notify stale players.
  for (int player_id : stale_players) {
    Observer* player = id_map_.Lookup(player_id);
    if (player && idle_player_map_.erase(player_id)) {
      stale_players_.insert(player_id);
      player->OnIdleTimeout();
    }
  }
}

void RendererWebMediaPlayerDelegate::OnDestruct() {
  delete this;
}

}  // namespace media
