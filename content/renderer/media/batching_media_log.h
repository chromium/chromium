// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/media_log.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}

namespace content {

// BatchingMediaLog is an implementation of MediaLog that sends messages
// grouped together in order to reduce IPC pressure.
// In order to subclass it, a subclass of the BatchingMediaLog::EventHandler
// should implement behavior when recieving a group of messages.
//
// It must be constructed on the render thread.
class CONTENT_EXPORT BatchingMediaLog : public media::MediaLog {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() = default;
    virtual void SendQueuedMediaEvents(std::vector<media::MediaLogRecord>) = 0;
    virtual void OnWebMediaPlayerDestroyed() = 0;
  };

  BatchingMediaLog(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   std::vector<std::unique_ptr<EventHandler>> impl);

  BatchingMediaLog(const BatchingMediaLog&) = delete;
  BatchingMediaLog& operator=(const BatchingMediaLog&) = delete;

  ~BatchingMediaLog() override;

  void Stop() override;

  // Will reset |last_ipc_send_time_| with the value of NowTicks().
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 protected:
  // MediaLog implementation.
  void AddLogRecordLocked(
      std::unique_ptr<media::MediaLogRecord> event) override;
  void OnWebMediaPlayerDestroyedLocked() override;
  std::string GetErrorMessageLocked() override;

 private:
  // Posted as a delayed task on |task_runner_| to throttle ipc message
  // frequency.
  void SendQueuedMediaEvents();

  void MaybeQueueEvent_Locked(std::unique_ptr<media::MediaLogRecord> event);

  std::string MediaEventToMessageString(const media::MediaLogRecord& event);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // |lock_| protects access to all of the following member variables.  It
  // allows any render process thread to AddEvent(), while preserving their
  // sequence for throttled send on |task_runner_| and coherent retrieval by
  // GetErrorMessage().  This is needed in addition to the synchronization
  // guarantees provided by MediaLog, since SendQueuedMediaEvents must also
  // be synchronized with respect to AddEvent.
  mutable base::Lock lock_;
  raw_ptr<const base::TickClock> tick_clock_ GUARDED_BY(lock_);
  base::TimeTicks last_ipc_send_time_ GUARDED_BY(lock_);
  std::vector<media::MediaLogRecord> queued_media_events_ GUARDED_BY(lock_);

  // impl for sending queued events.
  std::vector<std::unique_ptr<EventHandler>> event_handlers_ GUARDED_BY(lock_);

  // For enforcing max 1 pending send.
  bool ipc_send_pending_ GUARDED_BY(lock_);

  // True if we've logged a warning message about exceeding rate limits.
  bool logged_rate_limit_warning_ GUARDED_BY(lock_);

  // Limits the number of events we send over IPC to one.
  std::optional<media::MediaLogRecord> last_duration_changed_event_
      GUARDED_BY(lock_);
  std::optional<media::MediaLogRecord> last_buffering_state_event_
      GUARDED_BY(lock_);
  std::optional<media::MediaLogRecord> last_play_event_;
  std::optional<media::MediaLogRecord> last_pause_event_;

  // Holds the earliest MEDIA_ERROR_LOG_ENTRY event added to this log. This is
  // most likely to contain the most specific information available describing
  // any eventual fatal error.
  // TODO(wolenetz): Introduce a reset method to clear this in cases like
  // non-fatal error recovery like decoder fallback.
  std::optional<media::MediaLogRecord> cached_media_error_for_message_;

  // Holds a copy of the most recent PIPELINE_ERROR, if any.
  std::optional<media::MediaLogRecord> last_pipeline_error_;

  base::WeakPtr<BatchingMediaLog> weak_this_;
  base::WeakPtrFactory<BatchingMediaLog> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_
