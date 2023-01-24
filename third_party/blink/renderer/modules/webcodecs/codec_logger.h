// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_

#include <memory>
#include <string>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

namespace internal {

void SendPlayerNameInformationInternal(media::MediaLog* media_log,
                                       const ExecutionContext& context,
                                       std::string loadedAs);

}  // namespace internal

// Simple wrapper around MediaLog instances, to manage the lifetime safety of
// said MediaLogs. |parent_media_log_| must be destroyed and created on the
// main thread (or the worker thread if we are in a worker context).
// |media_log_| is a clone of |parent_media_log_| which can be safely passed to
// any thread. If the parent log is destroyed, |media_log_| will safely and
// silently stop logging.
// Note: Owners of this class should be ExecutionLifeCycleObservers, and should
// call Neuter() if the ExecutionContext passed to the constructor is destroyed.
template <typename StatusImpl>
class MODULES_EXPORT CodecLogger final {
 public:
  // Attempts to create CodecLogger backed by a BatchingMediaLog. Falls back to
  // a NullMediaLog on failure.
  CodecLogger(ExecutionContext* context,
              scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(context);

    // Owners of |this| should be ExecutionLifeCycleObservers, and should call
    // Neuter() if |context| is destroyed. The MediaInspectorContextImpl must
    // outlive |parent_media_log_|. If |context| is already destroyed, owners
    // might never call Neuter(), and MediaInspectorContextImpl* could be
    // garbage collected before |parent_media_log_| is destroyed.
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

    task_runner_ = task_runner;
  }

  ~CodecLogger() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // media logs must be posted for destruction, since they can cause the
    // garbage collector to trigger an immediate cleanup and delete the owning
    // instance of |CodecLogger|.
    if (parent_media_log_) {
      parent_media_log_->Stop();
      if (base::FeatureList::IsEnabled(
              features::kUseBlinkSchedulerTaskRunnerWithCustomDeleter)) {
        task_runner_->DeleteSoon(FROM_HERE, std::move(parent_media_log_));
      } else {
        // This task runner may be destroyed without running tasks, so don't use
        // DeleteSoon() which can leak the log. See https://crbug.com/1376851.
        task_runner_->PostTask(FROM_HERE, base::DoNothingWithBoundArgs(
                                              std::move(parent_media_log_)));
      }
    }
  }

  void SendPlayerNameInformation(const ExecutionContext& context,
                                 std::string loadedAs) {
    internal::SendPlayerNameInformationInternal(media_log_.get(), context,
                                                loadedAs);
  }

  // Creates an OperationError DOMException with the given |error_msg|, and logs
  // the given |status| in |media_log_|.
  // Since |status| can come from platform codecs, its contents won't be
  // surfaced to JS, since we could leak important information.
  DOMException* MakeException(std::string error_msg, StatusImpl status) {
    media_log_->NotifyError(status);

    if (!status_code_)
      status_code_ = status.code();

    return MakeGarbageCollected<DOMException>(DOMExceptionCode::kOperationError,
                                              error_msg.c_str());
  }

  // Convenience wrapper for MakeException(), where |error_msg| is shared for
  // both the exception message and the status message.
  DOMException* MakeException(
      std::string error_msg,
      typename StatusImpl::Codes code,
      const base::Location& location = base::Location::Current()) {
    return MakeException(error_msg, StatusImpl(code, error_msg, location));
  }

  // Safe to use on any thread. |this| should still outlive users of log().
  media::MediaLog* log() { return media_log_.get(); }

  // Destroys |parent_media_log_|, which makes |media_log_| silently stop
  // logging in a thread safe way.
  // Must be called if the ExecutionContext passed into the constructor is
  // destroyed.
  void Neuter() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    parent_media_log_ = nullptr;
  }

  // Records the first media::Status passed to MakeException.
  typename StatusImpl::Codes status_code() const {
    return status_code_.value_or(StatusImpl::Codes::kOk);
  }

 private:
  absl::optional<typename StatusImpl::Codes> status_code_;

  // |parent_media_log_| must be destroyed if ever the ExecutionContext is
  // destroyed, since the blink::MediaInspectorContext* pointer given to
  // InspectorMediaEventHandler might no longer be valid.
  // |parent_media_log_| should not be used directly. Use |media_log_| instead.
  std::unique_ptr<media::MediaLog> parent_media_log_;

  // We might destroy |parent_media_log_| at any point, so keep a clone which
  // can be safely accessed, and whose raw pointer can be given callbacks.
  std::unique_ptr<media::MediaLog> media_log_;

  // Keep task runner around for posting the media log to upon destruction.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_LOGGER_H_
