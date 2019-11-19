// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
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
    virtual void SendQueuedMediaEvents(std::vector<media::MediaLogEvent>) = 0;
    virtual void OnWebMediaPlayerDestroyed() = 0;
  };

  BatchingMediaLog(const GURL& security_origin,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   std::unique_ptr<EventHandler> impl);
  ~BatchingMediaLog() override;

  // Will reset |last_ipc_send_time_| with the value of NowTicks().
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 protected:
  // MediaLog implementation.
  void AddEventLocked(std::unique_ptr<media::MediaLogEvent> event) override;
  void OnWebMediaPlayerDestroyedLocked() override;
  std::string GetErrorMessageLocked() override;

 private:
  // Posted as a delayed task on |task_runner_| to throttle ipc message
  // frequency.
  void SendQueuedMediaEvents();

  // Security origin of the current frame.
  const GURL security_origin_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // impl for sending queued events.
  std::unique_ptr<EventHandler> event_handler_;

  // |lock_| protects access to all of the following member variables.  It
  // allows any render process thread to AddEvent(), while preserving their
  // sequence for throttled send on |task_runner_| and coherent retrieval by
  // GetErrorMessage().  This is needed in addition to the synchronization
  // guarantees provided by MediaLog, since SendQueuedMediaEvents must also
  // be synchronized with respect to AddEvent.
  mutable base::Lock lock_;
  const base::TickClock* tick_clock_;
  base::TimeTicks last_ipc_send_time_;
  std::vector<media::MediaLogEvent> queued_media_events_;

  // For enforcing max 1 pending send.
  bool ipc_send_pending_;

  // Limits the number of events we send over IPC to one.
  std::unique_ptr<media::MediaLogEvent> last_duration_changed_event_;

  // Holds the earliest MEDIA_ERROR_LOG_ENTRY event added to this log. This is
  // most likely to contain the most specific information available describing
  // any eventual fatal error.
  // TODO(wolenetz): Introduce a reset method to clear this in cases like
  // non-fatal error recovery like decoder fallback.
  std::unique_ptr<media::MediaLogEvent> cached_media_error_for_message_;

  // Holds a copy of the most recent PIPELINE_ERROR, if any.
  std::unique_ptr<media::MediaLogEvent> last_pipeline_error_;

  base::WeakPtr<BatchingMediaLog> weak_this_;
  base::WeakPtrFactory<BatchingMediaLog> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BatchingMediaLog);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_BATCHING_MEDIA_LOG_H_
