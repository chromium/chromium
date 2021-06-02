// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_

#include <memory>

#include "media/base/media_log.h"
#include "media/base/status.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class ExecutionContext;

// Simple wrapper around MediaLog instances, to manage the lifetime safety of
// said MediaLogs. |parent_media_log_| must be destroyed and created on the
// main thread (or the worker thread if we are in a worker context).
// |media_log_| is a clone of |parent_media_log_| which can be safely passed to
// any thread. If the parent log is destroyed, |media_log_| will safely and
// silently stop logging.
// Note: Owners of this class should be ExecutionLifeCycleObservers, and should
// call Neuter() if the ExecutionContext passed to the constructor is destroyed.
class MODULES_EXPORT CodecLogger final {
 public:
  // Attempts to create CodecLogger backed by a BatchingMediaLog. Falls back to
  // a NullMediaLog on failure.
  CodecLogger(ExecutionContext*,
              scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~CodecLogger();

  // Creates an OperationError DOMException with the given |error_msg|, and logs
  // the given |status| in |media_log_|.
  // Since |status| can come from platform codecs, its contents won't be
  // surfaced to JS, since we could leak important information.
  DOMException* MakeException(std::string error_msg, media::Status status);

  // Convenience wrapper for MakeException(), where |error_msg| is shared for
  // both the exception message and the status message.
  DOMException* MakeException(
      std::string error_msg,
      media::StatusCode code,
      const base::Location& location = base::Location::Current());

  // Safe to use on any thread. |this| should still outlive users of log().
  media::MediaLog* log() { return media_log_.get(); }

  // Destroys |parent_media_log_|, which makes |media_log_| silently stop
  // logging in a thread safe way.
  // Must be called if the ExecutionContext passed into the constructor is
  // destroyed.
  void Neuter();

  // Records the first media::Status passed to MakeException.
  media::StatusCode status_code() const { return status_code_; }

 private:
  media::StatusCode status_code_ = media::StatusCode::kOk;

  // |parent_media_log_| must be destroyed if ever the ExecutionContext is
  // destroyed, since the blink::MediaInspectorContext* pointer given to
  // InspectorMediaEventHandler might no longer be valid.
  // |parent_media_log_| should not be used directly. Use |media_log_| instead.
  std::unique_ptr<media::MediaLog> parent_media_log_;

  // We might destroy |parent_media_log_| at any point, so keep a clone which
  // can be safely accessed, and whose raw pointer can be given callbacks.
  std::unique_ptr<media::MediaLog> media_log_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_
