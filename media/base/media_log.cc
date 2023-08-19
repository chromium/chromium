// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "media/base/media_switches.h"

namespace media {

// Static declaration for dictionary keys that we expect to be used inside
// different |MediaLogRecord|s. We declare them here so if they change, its
// only in one spot.
const char MediaLog::kEventKey[] = "event";

MediaLog::MediaLog() : MediaLog(new ParentLogRecord(this)) {}

MediaLog::MediaLog(scoped_refptr<ParentLogRecord> parent_log_record)
    : parent_log_record_(std::move(parent_log_record)) {}

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
  InvalidateLog();
}

// Default *Locked implementations
void MediaLog::AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) {}

std::string MediaLog::GetErrorMessageLocked() {
  return "";
}

// Default implementation.
void MediaLog::Stop() {}

bool MediaLog::ShouldLogToDebugConsole() const {
#if DCHECK_IS_ON()
  return true;
#else
  return false;
#endif
}

void MediaLog::AddMessage(MediaLogMessageLevel level, std::string message) {
  std::unique_ptr<MediaLogRecord> record(
      CreateRecord(MediaLogRecord::Type::kMessage));
  if (!base::IsStringUTF8AllowingNoncharacters(message))
    message = "WARNING: system message could not be rendered!";
  record->params.Set(MediaLogMessageLevelToString(level), std::move(message));
  AddLogRecord(std::move(record));
}

void MediaLog::OnWebMediaPlayerDestroyedLocked() {}
void MediaLog::OnWebMediaPlayerDestroyed() {
  AddEvent<MediaLogEvent::kWebMediaPlayerDestroyed>();
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->OnWebMediaPlayerDestroyedLocked();
}

std::string MediaLog::GetErrorMessage() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    return parent_log_record_->media_log->GetErrorMessageLocked();

  return "";
}

std::unique_ptr<MediaLog> MediaLog::Clone() {
  // Protected ctor, so we can't use std::make_unique.
  return base::WrapUnique(new MediaLog(parent_log_record_));
}

void MediaLog::AddLogRecord(std::unique_ptr<MediaLogRecord> record) {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->AddLogRecordLocked(std::move(record));
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateRecord(
    MediaLogRecord::Type type) {
  auto record = std::make_unique<MediaLogRecord>();
  // Record IDs are populated by event handlers before they are sent to various
  // log viewers, such as the media-internals page, or devtools.
  record->id = 0;
  record->type = type;
  record->time = base::TimeTicks::Now();
  return record;
}

void MediaLog::InvalidateLog() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Do nothing if this log didn't create the record, i.e.
  // it's not the parent log. The parent log should invalidate itself.
  if (parent_log_record_->media_log == this)
    parent_log_record_->media_log = nullptr;
  // Keep |parent_log_record_| around, since the lock must keep working.
}

MediaLog::ParentLogRecord::ParentLogRecord(MediaLog* log) : media_log(log) {}
MediaLog::ParentLogRecord::~ParentLogRecord() = default;

LogHelper::LogHelper(MediaLogMessageLevel level,
                     MediaLog* media_log,
                     const char* file,
                     int line)
    : file_(file), line_(line), level_(level), media_log_(media_log) {
  DCHECK(media_log_);
}

LogHelper::LogHelper(MediaLogMessageLevel level,
                     const std::unique_ptr<MediaLog>& media_log,
                     const char* file,
                     int line)
    : LogHelper(level, media_log.get(), file, line) {}

LogHelper::~LogHelper() {
  const auto log = stream_.str();

  if (media_log_->ShouldLogToDebugConsole()) {
    switch (level_) {
      case MediaLogMessageLevel::kERROR:
        if (DLOG_IS_ON(ERROR)) {
          logging::LogMessage(file_, line_, logging::LOG_ERROR).stream() << log;
        }
        break;
      case MediaLogMessageLevel::kWARNING:
        if (DLOG_IS_ON(WARNING)) {
          logging::LogMessage(file_, line_, logging::LOG_WARNING).stream()
              << log;
        }
        break;
      case MediaLogMessageLevel::kINFO:
      case MediaLogMessageLevel::kDEBUG:
        if (DLOG_IS_ON(INFO)) {
          logging::LogMessage(file_, line_, logging::LOG_INFO).stream() << log;
        }
        break;
    }
  }

  media_log_->AddMessage(level_, log);
}

}  // namespace media
