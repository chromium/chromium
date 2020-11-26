// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_logger.h"

#include <string>

#include "media/base/media_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

namespace blink {

CodecLogger::CodecLogger(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(context);
  parent_media_log_ = Platform::Current()->GetMediaLog(
      MediaInspectorContextImpl::From(*context), task_runner);

  if (!parent_media_log_)
    parent_media_log_ = std::make_unique<media::NullMediaLog>();

  // This allows us to destroy |parent_media_log_| and stop logging,
  // without causing problems to |media_log_| users.
  media_log_ = parent_media_log_->Clone();
}

CodecLogger::CodecLogger()
    : parent_media_log_(std::make_unique<media::NullMediaLog>()),
      media_log_(parent_media_log_->Clone()) {}

DOMException* CodecLogger::MakeException(std::string error_msg,
                                         media::Status status) {
  media_log_->NotifyError(status);

  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kOperationError,
                                            error_msg.c_str());
}

DOMException* CodecLogger::MakeException(std::string error_msg,
                                         media::StatusCode code,
                                         const base::Location& location) {
  return MakeException(error_msg, media::Status(code, error_msg, location));
}

CodecLogger::~CodecLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CodecLogger::Neuter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parent_media_log_ = nullptr;
}

}  // namespace blink
