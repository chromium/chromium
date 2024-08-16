// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_web_media_player_delegate.h"

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics_action.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/media_content_type.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"
#include "ui/gfx/geometry/size.h"

namespace {

void RecordAction(const base::UserMetricsAction& action) {
  content::RenderThread::Get()->RecordAction(action);
}

}  // namespace

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      blink::WebViewObserver(render_frame->GetWebView()),
      allow_idle_cleanup_(
          content::GetContentClient()->renderer()->IsIdleMediaSuspendEnabled()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  idle_cleanup_interval_ = base::Seconds(5);
  idle_timeout_ = base::Seconds(15);

  // This imposes drastic limits on the number of media players and is only
  // appropriate for true low end devices.
  is_low_end_ = base::SysInfo::IsLowEndDevice();

  idle_cleanup_timer_.SetTaskRunner(
      render_frame->GetTaskRunner(blink::TaskType::kInternalMedia));
}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

bool RendererWebMediaPlayerDelegate::IsPageHidden() {
  // There is always a render frame except perhaps during teardown (though
  // |this| should be deleted before that would be observable).
  if (!render_frame())
    return true;

  // If the view is gone it means we are tearing down.
  if (!render_frame()->GetWebView())
    return true;

  switch (render_frame()->GetWebView()->GetVisibilityState()) {
    case blink::mojom::PageVisibilityState::kVisible:
    case blink::mojom::PageVisibilityState::kHiddenButPainting:
      return false;
    case blink::mojom::PageVisibilityState::kHidden:
      return true;
  }
  NOTREACHED();
}

bool RendererWebMediaPlayerDelegate::IsFrameHidden() {
  // There is always a render frame except perhaps during teardown (though
  // `this` should be deleted before that would be observable).
  CHECK(render_frame());

  // If the view is gone it means we are tearing down.
  if (!render_frame()->GetWebView()) {
    return true;
  }

  return is_frame_hidden_;
}

int RendererWebMediaPlayerDelegate::AddObserver(Observer* observer) {
  return id_map_.Add(observer);
}

void RendererWebMediaPlayerDelegate::RemoveObserver(int player_id) {
  DCHECK(id_map_.Lookup(player_id));
  id_map_.Remove(player_id);
  idle_player_map_.erase(player_id);
  stale_players_.erase(player_id);
  players_with_video_.erase(player_id);
  playing_videos_.erase(player_id);

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::DidMediaMetadataChange(
    int player_id,
    bool has_audio,
    bool has_video,
    MediaContentType media_content_type) {
  DVLOG(2) << __func__ << "(" << player_id << ", " << has_video << ", "
           << has_audio << ", " << static_cast<int>(media_content_type) << ")";

  if (has_video) {
    players_with_video_.insert(player_id);
  } else {
    players_with_video_.erase(player_id);
    playing_videos_.erase(player_id);
  }

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::DidPlay(int player_id) {
  DVLOG(2) << __func__ << "(" << player_id << ")";
  DCHECK(id_map_.Lookup(player_id));

  has_played_media_ = true;
  if (players_with_video_.count(player_id) == 1) {
    playing_videos_.insert(player_id);
    has_played_video_ = true;
  }

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::DidPause(int player_id,
                                              bool reached_end_of_stream) {
  DVLOG(2) << __func__ << "(" << player_id << ", " << reached_end_of_stream
           << ")";
  DCHECK(id_map_.Lookup(player_id));
  playing_videos_.erase(player_id);

  // Required to keep background playback statistics up to date.
  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::PlayerGone(int player_id) {
  DVLOG(2) << __func__ << "(" << player_id << ")";
  DCHECK(id_map_.Lookup(player_id));
  players_with_video_.erase(player_id);
  playing_videos_.erase(player_id);

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

void RendererWebMediaPlayerDelegate::OnPageVisibilityChanged(
    blink::mojom::PageVisibilityState visibility_state) {
  // Treat 'hidden but painting' as 'visible', since whatever is consuming the
  // painted output (e.g., Picture in Picture), probably wants the video.
  // Otherwise, the player might optimize the video away.
  bool is_shown;
  switch (visibility_state) {
    case blink::mojom::PageVisibilityState::kVisible:
    case blink::mojom::PageVisibilityState::kHiddenButPainting:
      is_shown = true;
      break;
    case blink::mojom::PageVisibilityState::kHidden:
      is_shown = false;
      break;
  }

  if (is_shown_.has_value() && *is_shown_ == is_shown)
    return;
  is_shown_ = is_shown;

  if (is_shown) {
    RecordAction(base::UserMetricsAction("Media.Shown"));

    for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->OnPageShown();
    }
  } else {
    RecordAction(base::UserMetricsAction("Media.Hidden"));

    for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->OnPageHidden();
    }
  }

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::SetIdleCleanupParamsForTesting(
    base::TimeDelta idle_timeout,
    base::TimeDelta idle_cleanup_interval,
    const base::TickClock* tick_clock,
    bool is_low_end) {
  idle_cleanup_interval_ = idle_cleanup_interval;
  idle_timeout_ = idle_timeout;
  tick_clock_ = tick_clock;
  is_low_end_ = is_low_end;
}

bool RendererWebMediaPlayerDelegate::IsIdleCleanupTimerRunningForTesting()
    const {
  return idle_cleanup_timer_.IsRunning();
}

void RendererWebMediaPlayerDelegate::SetFrameHiddenForTesting(
    bool is_frame_hidden) {
  if (is_frame_hidden == is_frame_hidden_) {
    return;
  }

  is_frame_hidden_ = is_frame_hidden;

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::ScheduleUpdateTask() {
  if (!pending_update_task_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RendererWebMediaPlayerDelegate::UpdateTask,
                                  weak_ptr_factory_.GetWeakPtr()));
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

  if (!allow_idle_cleanup_)
    return;

  // Clean up idle players.
  bool aggressive_cleanup = false;

  // When we reach the maximum number of idle players, clean them up
  // aggressively. Values chosen after testing on a Galaxy Nexus device for
  // http://crbug.com/612909.
  if (idle_player_map_.size() > (is_low_end_ ? 2u : 8u))
    aggressive_cleanup = true;

  // When a player plays on a buggy old device, clean up idle players
  // aggressively.
  if (has_played_video_since_last_update_task && is_low_end_)
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

void RendererWebMediaPlayerDelegate::OnFrameVisibilityChanged(
    blink::mojom::FrameVisibility render_status) {
  bool is_frame_hidden =
      (render_status == blink::mojom::FrameVisibility::kNotRendered);
  if (is_frame_hidden == is_frame_hidden_) {
    return;
  }

  is_frame_hidden_ = is_frame_hidden;
  if (is_frame_hidden_) {
    for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->OnFrameHidden();
    }
  } else {
    for (base::IDMap<Observer*>::iterator it(&id_map_); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->OnFrameShown();
    }
  }

  ScheduleUpdateTask();
}

void RendererWebMediaPlayerDelegate::OnDestruct() {
  // All WebMediaPlayer instances should have been destructed by this point.
  CHECK(id_map_.IsEmpty());
  CHECK(idle_player_map_.empty());
  CHECK(stale_players_.empty());
  CHECK(players_with_video_.empty());
  CHECK(playing_videos_.empty());
  delete this;
}

}  // namespace media
