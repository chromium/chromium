// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/batching_media_log.h"

#include <sstream>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "media/base/logging_override_if_enabled.h"

namespace {

// Keep the JSON conversion in one function to prevent LOG and DVLOG calls
// from unnecessarily converting it.
std::string ToJSON(const media::MediaLogRecord& event) {
  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return params_json;
}

// Print an event to the chromium log.
// TODO(tmathmeyer) replace this with a log-only EventHandler.
void Log(const media::MediaLogRecord& event) {
  if (event.type == media::MediaLogRecord::Type::kMediaStatus) {
    DVLOG(1) << "MediaEvent: " << ToJSON(event);
  } else if (event.type == media::MediaLogRecord::Type::kMessage &&
             event.params.HasKey("error")) {
    DVLOG(1) << "MediaEvent: " << ToJSON(event);
  } else if (event.type != media::MediaLogRecord::Type::kMediaPropertyChange) {
    DVLOG(1) << "MediaEvent: " << ToJSON(event);
  }
}

// This string comes from the TypeName template specialization
// in media_log_type_enforcement.h, it's not encoded anywhere, so it's
// just typed out here.
constexpr char kDurationChangedMessage[] = "kDurationChanged";
constexpr char kBufferingStateChangedMessage[] = "kBufferingStateChanged";

}  // namespace

namespace content {

BatchingMediaLog::BatchingMediaLog(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::vector<std::unique_ptr<EventHandler>> event_handlers)
    : task_runner_(std::move(task_runner)),
      event_handlers_(std::move(event_handlers)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      last_ipc_send_time_(tick_clock_->NowTicks()),
      ipc_send_pending_(false),
      logged_rate_limit_warning_(false) {
  // Pre-bind the WeakPtr on the right thread since we'll receive calls from
  // other threads and don't want races.
  weak_this_ = weak_factory_.GetWeakPtr();
}

BatchingMediaLog::~BatchingMediaLog() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // AddEvent() could be in-flight on some other thread.  Wait for it, and make
  // sure that nobody else calls it.
  InvalidateLog();

  // There's no further chance to handle this, so send them now. This should not
  // be racy since nothing should have a pointer to the media log on another
  // thread by this point.
  if (ipc_send_pending_)
    SendQueuedMediaEvents();
}

void BatchingMediaLog::OnWebMediaPlayerDestroyedLocked() {
  for (const auto& handler : event_handlers_)
    handler->OnWebMediaPlayerDestroyed();
}

void BatchingMediaLog::AddLogRecordLocked(
    std::unique_ptr<media::MediaLogRecord> event) {
  Log(*event);

  // For enforcing delay until it's been a second since the last ipc message was
  // sent.
  base::TimeDelta delay_for_next_ipc_send;
  {
    base::AutoLock auto_lock(lock_);
    switch (event->type) {
      // Hold onto the most recent PIPELINE_ERROR and the first, if any,
      // MEDIA_LOG_ERROR_ENTRY for use in GetErrorMessage().
      case media::MediaLogRecord::Type::kMediaStatus:
        last_pipeline_error_ = *event;
        MaybeQueueEvent_Locked(std::move(event));
        break;

      case media::MediaLogRecord::Type::kMediaEventTriggered: {
        DCHECK(event->params.HasKey(MediaLog::kEventKey));
        const auto* event_key =
            event->params.FindStringKey(MediaLog::kEventKey);
        if (*event_key == kDurationChangedMessage) {
          // This may fire many times for badly muxed media; only keep the last.
          last_duration_changed_event_ = *event;
        } else if (*event_key == kBufferingStateChangedMessage) {
          // This may fire many times on poor networks; only keep the last.
          last_buffering_state_event_ = *event;
        } else {
          MaybeQueueEvent_Locked(std::move(event));
        }
        break;
      }

      case media::MediaLogRecord::Type::kMessage:
        if (event->params.HasKey(media::MediaLogMessageLevelToString(
                media::MediaLogMessageLevel::kERROR)) &&
            !cached_media_error_for_message_) {
          cached_media_error_for_message_ = *event;
        }
        MaybeQueueEvent_Locked(std::move(event));
        break;

      default:
        MaybeQueueEvent_Locked(std::move(event));
    }

    if (ipc_send_pending_)
      return;

    ipc_send_pending_ = true;
    delay_for_next_ipc_send = base::TimeDelta::FromSeconds(1) -
                              (tick_clock_->NowTicks() - last_ipc_send_time_);
  }

  if (delay_for_next_ipc_send > base::TimeDelta()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BatchingMediaLog::SendQueuedMediaEvents, weak_this_),
        delay_for_next_ipc_send);
    return;
  }

  // It's been more than a second so send ASAP.
  if (task_runner_->BelongsToCurrentThread()) {
    SendQueuedMediaEvents();
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BatchingMediaLog::SendQueuedMediaEvents, weak_this_));
}

std::string BatchingMediaLog::GetErrorMessageLocked() {
  // Keep message structure in sync with
  // HTMLMediaElement::BuildElementErrorMessage().
  std::stringstream result;
  if (last_pipeline_error_)
    result << MediaEventToMessageString(*last_pipeline_error_);

  if (cached_media_error_for_message_) {
    DCHECK(last_pipeline_error_)
        << "Message with detail should be associated with a pipeline error";
    // This ':' lets web apps extract the UA-specific-error-code from the
    // MediaError.message prefix.
    result << ": "
           << MediaEventToMessageString(*cached_media_error_for_message_);
  }

  return result.str();
}

std::string BatchingMediaLog::MediaEventToMessageString(
    const media::MediaLogRecord& event) {
  switch (event.type) {
    case media::MediaLogRecord::Type::kMediaStatus: {
      int error_code = 0;
      event.params.GetInteger(media::MediaLog::kStatusText, &error_code);
      DCHECK_NE(error_code, 0);
      return PipelineStatusToString(
          static_cast<media::PipelineStatus>(error_code));
    }
    case media::MediaLogRecord::Type::kMessage: {
      std::string result;
      if (event.params.GetString(
              MediaLogMessageLevelToString(media::MediaLogMessageLevel::kERROR),
              &result)) {
        base::ReplaceChars(result, "\n", " ", &result);
      }
      return result;
    }
    default:
      NOTREACHED();
      return "";
  }
}

void BatchingMediaLog::SendQueuedMediaEvents() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::vector<media::MediaLogRecord> events_to_send;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(ipc_send_pending_);
    ipc_send_pending_ = false;

    if (last_duration_changed_event_) {
      queued_media_events_.push_back(*last_duration_changed_event_);
      last_duration_changed_event_.reset();
    }

    if (last_buffering_state_event_) {
      queued_media_events_.push_back(*last_buffering_state_event_);
      last_buffering_state_event_.reset();
    }

    queued_media_events_.swap(events_to_send);
    last_ipc_send_time_ = tick_clock_->NowTicks();
  }

  if (events_to_send.empty())
    return;

  for (const auto& handler : event_handlers_)
    handler->SendQueuedMediaEvents(events_to_send);
}

void BatchingMediaLog::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  base::AutoLock auto_lock(lock_);
  tick_clock_ = tick_clock;
  last_ipc_send_time_ = tick_clock_->NowTicks();
}

void BatchingMediaLog::MaybeQueueEvent_Locked(
    std::unique_ptr<media::MediaLogRecord> event) {
  lock_.AssertAcquired();
  if (queued_media_events_.size() < media::MediaLog::kLogLimit) {
    queued_media_events_.emplace_back(*event);
    return;
  }

  if (logged_rate_limit_warning_)
    return;

  logged_rate_limit_warning_ = true;

  auto message = "Log rate exceeds " +
                 base::NumberToString(media::MediaLog::kLogLimit) +
                 " messages per second. Futher entries will be dropped.";
  DLOG(WARNING) << message;

  queued_media_events_.emplace_back();
  queued_media_events_.back().id = event->id;
  queued_media_events_.back().type = media::MediaLogRecord::Type::kMessage;
  queued_media_events_.back().time = base::TimeTicks::Now();
  queued_media_events_.back().params.SetStringPath(
      media::MediaLogMessageLevelToString(
          media::MediaLogMessageLevel::kWARNING),
      message);
}

}  // namespace content
