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

const char MediaLog::kEventKey[] = "event";
const char MediaLog::kStatusText[] = "pipeline_error";

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::AtomicSequenceNumber g_media_log_count;

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

void MediaLog::AddMessage(MediaLogMessageLevel level, std::string message) {
  std::unique_ptr<MediaLogRecord> record(
      CreateRecord(MediaLogRecord::Type::kMessage));
  record->params.SetStringPath(MediaLogMessageLevelToString(level),
                               std::move(message));
  AddLogRecord(std::move(record));
}

void MediaLog::NotifyError(PipelineStatus status) {
  std::unique_ptr<MediaLogRecord> record(
      CreateRecord(MediaLogRecord::Type::kMediaStatus));
  record->params.SetIntPath(MediaLog::kStatusText, status);
  AddLogRecord(std::move(record));
}

void MediaLog::NotifyError(Status status) {
  DCHECK(!status.is_ok());
  std::string output_str;
  base::JSONWriter::Write(MediaSerialize(status), &output_str);
  AddMessage(MediaLogMessageLevel::kERROR, output_str);
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
  record->id = id();
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

MediaLog::ParentLogRecord::ParentLogRecord(MediaLog* log)
    : id(g_media_log_count.GetNext()), media_log(log) {}
MediaLog::ParentLogRecord::~ParentLogRecord() = default;

LogHelper::LogHelper(MediaLogMessageLevel level, MediaLog* media_log)
    : level_(level), media_log_(media_log) {
  DCHECK(media_log_);
}

LogHelper::LogHelper(MediaLogMessageLevel level,
                     const std::unique_ptr<MediaLog>& media_log)
    : LogHelper(level, media_log.get()) {}

LogHelper::~LogHelper() {
  media_log_->AddMessage(level_, stream_.str());
}

}  //namespace media
