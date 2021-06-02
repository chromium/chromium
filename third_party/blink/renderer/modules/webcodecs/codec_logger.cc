// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_logger.h"

#include <string>

#include "media/base/media_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

CodecLogger::CodecLogger(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(context);

  // Owners of |this| should be ExecutionLifeCycleObservers, and should call
  // Neuter() if |context| is destroyed. The MediaInspectorContextImpl must
  // outlive |parent_media_log_|. If |context| is already destroyed, owners
  // might never call Neuter(), and MediaInspectorContextImpl* could be garbage
  // collected before |parent_media_log_| is destroyed.
  if (!context->IsContextDestroyed()) {
    parent_media_log_ = Platform::Current()->GetMediaLog(
        MediaInspectorContextImpl::From(*context), task_runner,
        /*is_on_worker=*/!IsMainThread());
  }

  // NullMediaLog silently and safely does nothing.
  if (!parent_media_log_)
    parent_media_log_ = std::make_unique<media::NullMediaLog>();

  // This allows us to destroy |parent_media_log_| and stop logging,
  // without causing problems to |media_log_| users.
  media_log_ = parent_media_log_->Clone();
}

DOMException* CodecLogger::MakeException(std::string error_msg,
                                         media::Status status) {
  media_log_->NotifyError(status);

  if (status_code_ == media::StatusCode::kOk) {
    DCHECK(!status.is_ok());
    status_code_ = status.code();
  }

  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kOperationError,
                                            error_msg.c_str());
}

DOMException* CodecLogger::MakeException(std::string error_msg,
                                         media::StatusCode code,
                                         const base::Location& location) {
  if (status_code_ == media::StatusCode::kOk) {
    DCHECK_NE(code, media::StatusCode::kOk);
    status_code_ = code;
  }

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
