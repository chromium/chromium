// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"

#if defined(OS_ANDROID)
#include "base/time/time.h"
#endif  // OS_ANDROID

namespace blink {
enum class WebFullscreenVideoStatus;
}

namespace media {

enum class MediaContentType;

// Standard implementation of WebMediaPlayerDelegate; communicates state to
// the MediaPlayerDelegateHost.
class CONTENT_EXPORT RendererWebMediaPlayerDelegate
    : public content::RenderFrameObserver,
      public blink::WebMediaPlayerDelegate,
      public base::SupportsWeakPtr<RendererWebMediaPlayerDelegate> {
 public:
  explicit RendererWebMediaPlayerDelegate(content::RenderFrame* render_frame);
  ~RendererWebMediaPlayerDelegate() override;

  // Returns true if this RenderFrame has ever seen media playback before.
  bool has_played_media() const { return has_played_media_; }

  // blink::WebMediaPlayerDelegate implementation.
  bool IsFrameHidden() override;
  bool IsFrameClosed() override;
  int AddObserver(Observer* observer) override;
  void RemoveObserver(int player_id) override;
  void DidPlay(int player_id,
               bool has_video,
               bool has_audio,
               MediaContentType media_content_type) override;
  void DidPause(int player_id) override;
  void PlayerGone(int player_id) override;
  void SetIdle(int player_id, bool is_idle) override;
  bool IsIdle(int player_id) override;
  void ClearStaleFlag(int player_id) override;
  bool IsStale(int player_id) override;
  void SetIsEffectivelyFullscreen(
      int player_id,
      blink::WebFullscreenVideoStatus fullscreen_video_status) override;
  void DidPlayerSizeChange(int delegate_id, const gfx::Size& size) override;
  void DidPlayerMutedStatusChange(int delegate_id, bool muted) override;
  void DidPlayerMediaPositionStateChange(
      int delegate_id,
      const media_session::MediaPosition& position) override;

  // content::RenderFrameObserver overrides.
  void WasHidden() override;
  void WasShown() override;
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnDestruct() override;

  // Zeros out |idle_cleanup_interval_|, sets |idle_timeout_| to |idle_timeout|,
  // and |is_low_end_| to |is_low_end|. A zero cleanup interval
  // will cause the idle timer to run with each run of the message loop.
  void SetIdleCleanupParamsForTesting(base::TimeDelta idle_timeout,
                                      base::TimeDelta idle_cleanup_interval,
                                      const base::TickClock* tick_clock,
                                      bool is_low_end);
  bool IsIdleCleanupTimerRunningForTesting() const;

  // Note: Does not call OnFrameHidden()/OnFrameShown().
  void SetFrameHiddenForTesting(bool is_hidden);

  friend class RendererWebMediaPlayerDelegateTest;

 private:
  void OnMediaDelegatePause(int player_id, bool triggered_by_user);
  void OnMediaDelegatePlay(int player_id);
  void OnMediaDelegateMuted(int player_id, bool muted);
  void OnMediaDelegateSeekForward(int player_id, base::TimeDelta seek_time);
  void OnMediaDelegateSeekBackward(int player_id, base::TimeDelta seek_time);
  void OnMediaDelegateSuspendAllMediaPlayers();
  void OnMediaDelegateVolumeMultiplierUpdate(int player_id, double multiplier);
  void OnMediaDelegateBecamePersistentVideo(int player_id, bool value);

  // Schedules UpdateTask() to run soon.
  void ScheduleUpdateTask();

  // Processes state changes, dispatches CleanupIdlePlayers().
  void UpdateTask();

  // Records UMAs about background playback.
  void RecordBackgroundVideoPlayback();

  // Runs periodically to notify stale players in |idle_player_map_| which
  // have been idle for longer than |timeout|.
  void CleanUpIdlePlayers(base::TimeDelta timeout);

  // True if any media has ever been played in this render frame. Affects
  // autoplay logic in RenderFrameImpl.
  bool has_played_media_ = false;

  bool is_frame_closed_ = false;
  bool is_frame_hidden_for_testing_ = false;

  // State related to scheduling UpdateTask(). These are cleared each time
  // UpdateTask() runs.
  bool has_played_video_ = false;
  bool pending_update_task_ = false;

  base::IDMap<Observer*> id_map_;

  // Flag for gating if players should ever transition to a stale state after a
  // period of inactivity.
  bool allow_idle_cleanup_ = true;

  // Tracks which players have entered an idle state. After some period of
  // inactivity these players will be notified and become stale.
  std::map<int, base::TimeTicks> idle_player_map_;
  std::set<int> stale_players_;
  base::OneShotTimer idle_cleanup_timer_;

  // Amount of time allowed to elapse after a player becomes idle before
  // it can transition to stale.
  base::TimeDelta idle_timeout_;

  // The polling interval used for checking the players to see if any have
  // exceeded |idle_timeout_| since becoming idle.
  base::TimeDelta idle_cleanup_interval_;

  // Clock used for calculating when players have become stale. May be
  // overridden for testing.
  const base::TickClock* tick_clock_;

#if defined(OS_ANDROID)
  bool was_playing_background_video_ = false;

  // Keeps track of when the background video playback started for metrics.
  base::TimeTicks background_video_start_time_;
#endif  // OS_ANDROID

  // The currently playing local videos. Used to determine whether
  // OnMediaDelegatePlay() should allow the videos to play in the background or
  // not.
  std::set<int> playing_videos_;

  // Determined at construction time based on system information; determines
  // when the idle cleanup timer should be fired more aggressively.
  bool is_low_end_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegate);
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
