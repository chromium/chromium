// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEB_MEDIA_PLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEB_MEDIA_PLAYER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/media/web_media_player_delegate.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace blink {
enum class WebFullscreenVideoStatus;
}

namespace media {

enum class MediaContentType;

// Standard implementation of WebMediaPlayerDelegate; communicates state to
// the MediaPlayerDelegateHost.
class CONTENT_EXPORT RendererWebMediaPlayerDelegate final
    : public content::RenderFrameObserver,
      public blink::WebViewObserver,
      public blink::WebMediaPlayerDelegate {
 public:
  explicit RendererWebMediaPlayerDelegate(content::RenderFrame* render_frame);

  RendererWebMediaPlayerDelegate(const RendererWebMediaPlayerDelegate&) =
      delete;
  RendererWebMediaPlayerDelegate& operator=(
      const RendererWebMediaPlayerDelegate&) = delete;

  ~RendererWebMediaPlayerDelegate() override;

  // Returns true if this RenderFrame has ever seen media playback before.
  bool has_played_media() const { return has_played_media_; }

  // blink::WebMediaPlayerDelegate implementation.
  bool IsPageHidden() override;
  bool IsFrameHidden() override;
  int AddObserver(Observer* observer) override;
  void RemoveObserver(int player_id) override;
  void DidMediaMetadataChange(int player_id,
                              bool has_audio,
                              bool has_video,
                              MediaContentType media_content_type) override;
  void DidPlay(int player_id) override;
  void DidPause(int player_id, bool reached_end_of_stream) override;
  void PlayerGone(int player_id) override;
  void SetIdle(int player_id, bool is_idle) override;
  bool IsIdle(int player_id) override;
  void ClearStaleFlag(int player_id) override;
  bool IsStale(int player_id) override;

  // content::RenderFrameObserver overrides.
  void OnDestruct() override;
  void OnFrameVisibilityChanged(
      blink::mojom::FrameVisibility render_status) override;

  // blink::WebViewObserver overrides.
  void OnPageVisibilityChanged(
      blink::mojom::PageVisibilityState visibility_state) override;

  // Returns the number of WebMediaPlayers that are associated with this
  // delegate.
  size_t web_media_player_count() const { return id_map_.size(); }

  // Zeros out |idle_cleanup_interval_|, sets |idle_timeout_| to |idle_timeout|,
  // and |is_low_end_| to |is_low_end|. A zero cleanup interval
  // will cause the idle timer to run with each run of the message loop.
  void SetIdleCleanupParamsForTesting(base::TimeDelta idle_timeout,
                                      base::TimeDelta idle_cleanup_interval,
                                      const base::TickClock* tick_clock,
                                      bool is_low_end);
  bool IsIdleCleanupTimerRunningForTesting() const;

  // Note: Does not call OnFrameHidden()/OnFrameShown().
  void SetFrameHiddenForTesting(bool is_frame_hidden);

  friend class RendererWebMediaPlayerDelegateTest;

 private:
  // Schedules UpdateTask() to run soon.
  void ScheduleUpdateTask();

  // Processes state changes, dispatches CleanupIdlePlayers().
  void UpdateTask();

  // Runs periodically to notify stale players in |idle_player_map_| which
  // have been idle for longer than |timeout|.
  void CleanUpIdlePlayers(base::TimeDelta timeout);

  // True if any media has ever been played in this render frame. Affects
  // autoplay logic in RenderFrameImpl.
  bool has_played_media_ = false;

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
  raw_ptr<const base::TickClock> tick_clock_;

  // Players with a video track.
  base::flat_set<int> players_with_video_;

  // The currently playing local videos. Used to determine whether
  // OnMediaDelegatePlay() should allow the videos to play in the background or
  // not.
  base::flat_set<int> playing_videos_;

  // Determined at construction time based on system information; determines
  // when the idle cleanup timer should be fired more aggressively.
  bool is_low_end_;

  // Last page shown/hidden state sent to the player.  Unset if we have not sent
  // any message.  Used to elide duplicates.
  std::optional<bool> is_shown_;

  // Last rendered status sent to the player from the containing frame.
  bool is_frame_hidden_ = false;

  base::WeakPtrFactory<RendererWebMediaPlayerDelegate> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEB_MEDIA_PLAYER_DELEGATE_H_
