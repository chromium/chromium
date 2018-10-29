// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace media {

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::AtomicSequenceNumber g_media_log_count;

std::string MediaLog::MediaLogLevelToString(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return "error";
    case MEDIALOG_WARNING:
      return "warning";
    case MEDIALOG_INFO:
      return "info";
    case MEDIALOG_DEBUG:
      return "debug";
  }
  NOTREACHED();
  return NULL;
}

MediaLogEvent::Type MediaLog::MediaLogLevelToEventType(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return MediaLogEvent::MEDIA_ERROR_LOG_ENTRY;
    case MEDIALOG_WARNING:
      return MediaLogEvent::MEDIA_WARNING_LOG_ENTRY;
    case MEDIALOG_INFO:
      return MediaLogEvent::MEDIA_INFO_LOG_ENTRY;
    case MEDIALOG_DEBUG:
      return MediaLogEvent::MEDIA_DEBUG_LOG_ENTRY;
  }
  NOTREACHED();
  return MediaLogEvent::MEDIA_ERROR_LOG_ENTRY;
}

std::string MediaLog::EventTypeToString(MediaLogEvent::Type type) {
  switch (type) {
    case MediaLogEvent::WEBMEDIAPLAYER_CREATED:
      return "WEBMEDIAPLAYER_CREATED";
    case MediaLogEvent::WEBMEDIAPLAYER_DESTROYED:
      return "WEBMEDIAPLAYER_DESTROYED";
    case MediaLogEvent::LOAD:
      return "LOAD";
    case MediaLogEvent::SEEK:
      return "SEEK";
    case MediaLogEvent::PLAY:
      return "PLAY";
    case MediaLogEvent::PAUSE:
      return "PAUSE";
    case MediaLogEvent::PIPELINE_STATE_CHANGED:
      return "PIPELINE_STATE_CHANGED";
    case MediaLogEvent::PIPELINE_ERROR:
      return "PIPELINE_ERROR";
    case MediaLogEvent::VIDEO_SIZE_SET:
      return "VIDEO_SIZE_SET";
    case MediaLogEvent::DURATION_SET:
      return "DURATION_SET";
    case MediaLogEvent::ENDED:
      return "ENDED";
    case MediaLogEvent::TEXT_ENDED:
      return "TEXT_ENDED";
    case MediaLogEvent::MEDIA_ERROR_LOG_ENTRY:
      return "MEDIA_ERROR_LOG_ENTRY";
    case MediaLogEvent::MEDIA_WARNING_LOG_ENTRY:
      return "MEDIA_WARNING_LOG_ENTRY";
    case MediaLogEvent::MEDIA_INFO_LOG_ENTRY:
      return "MEDIA_INFO_LOG_ENTRY";
    case MediaLogEvent::MEDIA_DEBUG_LOG_ENTRY:
      return "MEDIA_DEBUG_LOG_ENTRY";
    case MediaLogEvent::PROPERTY_CHANGE:
      return "PROPERTY_CHANGE";
  }
  NOTREACHED();
  return NULL;
}

std::string MediaLog::PipelineStatusToString(PipelineStatus status) {
#define STRINGIFY_STATUS_CASE(status) \
  case status:                        \
    return #status

  switch (status) {
    STRINGIFY_STATUS_CASE(PIPELINE_OK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_NETWORK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_DECODE);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_ABORT);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INITIALIZATION_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_COULD_NOT_RENDER);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_READ);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INVALID_STATE);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_COULD_NOT_OPEN);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_COULD_NOT_PARSE);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_DETECTED_HLS);
    STRINGIFY_STATUS_CASE(DECODER_ERROR_NOT_SUPPORTED);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_APPEND_FAILED);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR);
    STRINGIFY_STATUS_CASE(AUDIO_RENDERER_ERROR);
  }

#undef STRINGIFY_STATUS_CASE

  NOTREACHED();
  return NULL;
}

std::string MediaLog::MediaEventToLogString(const MediaLogEvent& event) {
  // Special case for PIPELINE_ERROR, since that's by far the most useful
  // event for figuring out media pipeline failures, and just reporting
  // pipeline status as numeric code is not very helpful/user-friendly.
  int error_code = 0;
  if (event.type == MediaLogEvent::PIPELINE_ERROR &&
      event.params.GetInteger("pipeline_error", &error_code)) {
    PipelineStatus status = static_cast<PipelineStatus>(error_code);
    return EventTypeToString(event.type) + " " + PipelineStatusToString(status);
  }

  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return EventTypeToString(event.type) + " " + params_json;
}

std::string MediaLog::MediaEventToMessageString(const MediaLogEvent& event) {
  switch (event.type) {
    case MediaLogEvent::PIPELINE_ERROR: {
      int error_code = 0;
      event.params.GetInteger("pipeline_error", &error_code);
      DCHECK_NE(error_code, 0);
      return PipelineStatusToString(static_cast<PipelineStatus>(error_code));
    }
    case MediaLogEvent::MEDIA_ERROR_LOG_ENTRY: {
      std::string result = "";
      if (event.params.GetString(MediaLogLevelToString(MEDIALOG_ERROR),
                                 &result))
        base::ReplaceChars(result, "\n", " ", &result);
      return result;
    }
    default:
      NOTREACHED();
      return "";
  }
}

std::string MediaLog::BufferingStateToString(BufferingState state) {
  switch (state) {
    case BUFFERING_HAVE_NOTHING:
      return "BUFFERING_HAVE_NOTHING";
    case BUFFERING_HAVE_ENOUGH:
      return "BUFFERING_HAVE_ENOUGH";
  }
  NOTREACHED();
  return "";
}

MediaLog::MediaLog() : MediaLog(new ParentLogRecord(this)) {}

MediaLog::MediaLog(scoped_refptr<ParentLogRecord> parent_log_record)
    : parent_log_record_(std::move(parent_log_record)),
      id_(g_media_log_count.GetNext()) {}

MediaLog::~MediaLog() {
  // If we are the underlying log, then somebody should have called
  // InvalidateLog before now.  Otherwise, there could be concurrent calls into
  // this after we're destroyed.  Note that calling it here isn't really much
  // better, since there could be concurrent calls into the now destroyed
  // derived class.
  //
  // However, we can't DCHECK on it, since lots of folks create a base Medialog
  // implementation temporarily.  So, the best we can do is invalidate the log.
  // We could get around this if we introduce a new NullMediaLog that handles
  // log invalidation, so we could dcheck here.  However, that seems like a lot
  // of boilerplate.
  if (parent_log_record_->media_log == this)
    InvalidateLog();
}

void MediaLog::AddEvent(std::unique_ptr<MediaLogEvent> event) {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->AddEventLocked(std::move(event));
}

void MediaLog::AddEventLocked(std::unique_ptr<MediaLogEvent> event) {}

std::string MediaLog::GetErrorMessage() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    return parent_log_record_->media_log->GetErrorMessageLocked();

  return "";
}

std::string MediaLog::GetErrorMessageLocked() {
  return "";
}

void MediaLog::RecordRapporWithSecurityOrigin(const std::string& metric) {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->RecordRapporWithSecurityOriginLocked(metric);
}

void MediaLog::RecordRapporWithSecurityOriginLocked(const std::string& metric) {
  DVLOG(1) << "Default MediaLog doesn't support rappor reporting.";
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateCreatedEvent(
    const std::string& origin_url) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_CREATED));
  event->params.SetString("origin_url", TruncateUrlString(origin_url));
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateEvent(MediaLogEvent::Type type) {
  std::unique_ptr<MediaLogEvent> event(new MediaLogEvent);
  event->id = id_;
  event->type = type;
  event->time = base::TimeTicks::Now();
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateBooleanEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    bool value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetBoolean(property, value);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateStringEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    const std::string& value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetString(property, value);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateTimeEvent(
    MediaLogEvent::Type type,
    const std::string& property,
    base::TimeDelta value) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(type));
  if (value.is_max())
    event->params.SetString(property, "unknown");
  else
    event->params.SetDouble(property, value.InSecondsF());
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateLoadEvent(
    const std::string& url) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::LOAD));
  event->params.SetString("url", TruncateUrlString(url));
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateSeekEvent(double seconds) {
  std::unique_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::SEEK));
  event->params.SetDouble("seek_target", seconds);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreatePipelineStateChangedEvent(
    PipelineImpl::State state) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PIPELINE_STATE_CHANGED));
  event->params.SetString("pipeline_state",
                          PipelineImpl::GetStateString(state));
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreatePipelineErrorEvent(
    PipelineStatus error) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PIPELINE_ERROR));
  event->params.SetInteger("pipeline_error", error);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateVideoSizeSetEvent(
    size_t width,
    size_t height) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::VIDEO_SIZE_SET));
  event->params.SetInteger("width", width);
  event->params.SetInteger("height", height);
  return event;
}

std::unique_ptr<MediaLogEvent> MediaLog::CreateBufferingStateChangedEvent(
    const std::string& property,
    BufferingState state) {
  return CreateStringEvent(MediaLogEvent::PROPERTY_CHANGE, property,
                           BufferingStateToString(state));
}

void MediaLog::AddLogEvent(MediaLogLevel level, const std::string& message) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogLevelToEventType(level)));
  event->params.SetString(MediaLogLevelToString(level), message);
  AddEvent(std::move(event));
}

void MediaLog::SetStringProperty(
    const std::string& key, const std::string& value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetString(key, value);
  AddEvent(std::move(event));
}

void MediaLog::SetDoubleProperty(
    const std::string& key, double value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetDouble(key, value);
  AddEvent(std::move(event));
}

void MediaLog::SetBooleanProperty(
    const std::string& key, bool value) {
  std::unique_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetBoolean(key, value);
  AddEvent(std::move(event));
}

std::unique_ptr<MediaLog> MediaLog::Clone() {
  // Protected ctor, so we can't use std::make_unique.
  return base::WrapUnique(new MediaLog(parent_log_record_));
}

// static
std::string MediaLog::TruncateUrlString(std::string log_string) {
  if (log_string.length() > kMaxUrlLength) {
    log_string.resize(kMaxUrlLength);

    // Room for the ellipsis.
    DCHECK_GE(kMaxUrlLength, std::size_t{3});
    log_string.replace(log_string.end() - 3, log_string.end(), "...");
  }

  return log_string;
}

MediaLog::ParentLogRecord::ParentLogRecord(MediaLog* log) : media_log(log) {}
MediaLog::ParentLogRecord::~ParentLogRecord() = default;

void MediaLog::InvalidateLog() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // It's almost certainly unintentional to invalidate a parent log.
  DCHECK(parent_log_record_->media_log == nullptr ||
         parent_log_record_->media_log == this);

  parent_log_record_->media_log = nullptr;
  // Keep |parent_log_record_| around, since the lock must keep working.
}

LogHelper::LogHelper(MediaLog::MediaLogLevel level, MediaLog* media_log)
    : level_(level), media_log_(media_log) {
  DCHECK(media_log_);
}

LogHelper::LogHelper(MediaLog::MediaLogLevel level,
                     const std::unique_ptr<MediaLog>& media_log)
    : LogHelper(level, media_log.get()) {}

LogHelper::~LogHelper() {
  media_log_->AddLogEvent(level_, stream_.str());
}

}  //namespace media
