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

void MediaLog::EmitConsoleErrorLog(base::Value::Dict status_dict) {
  auto stack = status_dict.Extract(StatusConstants::kStackKey);
  DCHECK(stack);
  DCHECK(stack->is_list());
  DCHECK(!stack->GetList().empty());
  DCHECK(stack->GetList().front().is_dict());

  auto file =
      stack->GetList().front().GetDict().Extract(StatusConstants::kFileKey);
  DCHECK(file);
  DCHECK(file->is_string());

  auto line =
      stack->GetList().front().GetDict().Extract(StatusConstants::kLineKey);
  DCHECK(line);
  DCHECK(line->is_int());

  auto log_writer = logging::LogMessage(file->GetString().c_str(),
                                        line->GetInt(), logging::LOGGING_ERROR);
  if (auto message = status_dict.Extract(StatusConstants::kMsgKey);
      message && message->is_string()) {
    auto message_str = message->GetString();
    if (!message_str.empty()) {
      log_writer.stream() << message_str << ": ";
    }
  }
  log_writer.stream() << base::WriteJson(status_dict).value_or(std::string());
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
                     int line,
                     std::optional<logging::SystemErrorCode> code)
    : file_(file),
      line_(line),
      level_(level),
      media_log_(media_log),
      code_(code) {
  DCHECK(media_log_);
}

LogHelper::LogHelper(MediaLogMessageLevel level,
                     const std::unique_ptr<MediaLog>& media_log,
                     const char* file,
                     int line,
                     std::optional<logging::SystemErrorCode> code)
    : LogHelper(level, media_log.get(), file, line, code) {}

LogHelper::~LogHelper() {
  if (code_) {
    stream_ << ": ";
    auto err_string = logging::SystemErrorCodeToString(*code_);
    if (!base::IsStringUTF8AllowingNoncharacters(err_string)) {
      stream_ << *code_;
    } else {
      stream_ << err_string;
    }
  }

  const auto log = stream_.str();
  if (media_log_->ShouldLogToDebugConsole()) {
    switch (level_) {
      case MediaLogMessageLevel::kERROR:
        // ERRORs are always logged regardless of kMediaLogToConsole value.
        if (DLOG_IS_ON(ERROR)) {
          logging::LogMessage(file_, line_, logging::LOGGING_ERROR).stream()
              << log;
        }
        break;
      case MediaLogMessageLevel::kWARNING:
        if (DLOG_IS_ON(WARNING) &&
            base::FeatureList::IsEnabled(kMediaLogToConsole)) {
          logging::LogMessage(file_, line_, logging::LOGGING_WARNING).stream()
              << log;
        }
        break;
      case MediaLogMessageLevel::kINFO:
      case MediaLogMessageLevel::kDEBUG:
        if (DLOG_IS_ON(INFO) &&
            base::FeatureList::IsEnabled(kMediaLogToConsole)) {
          logging::LogMessage(file_, line_, logging::LOGGING_INFO).stream()
              << log;
        }
        break;
    }
  }

  media_log_->AddMessage(level_, log);
}

}  // namespace media
