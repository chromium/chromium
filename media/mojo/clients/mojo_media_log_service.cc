// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_media_log_service.h"

#include "base/logging.h"
#include "media/base/media_log_record.h"

namespace media {

MojoMediaLogService::MojoMediaLogService(
    std::unique_ptr<media::MediaLog> media_log)
    : media_log_(std::move(media_log)) {
  DVLOG(1) << __func__;
  DCHECK(media_log_);
}

MojoMediaLogService::~MojoMediaLogService() {
  DVLOG(1) << __func__;
}

void MojoMediaLogService::AddLogRecord(const MediaLogRecord& event) {
  DVLOG(1) << __func__;

  // Make a copy so that we can transfer ownership to |media_log_|.
  std::unique_ptr<media::MediaLogRecord> modified_event =
      std::make_unique<media::MediaLogRecord>(event);

  media_log_->AddLogRecord(std::move(modified_event));
}

}  // namespace media
