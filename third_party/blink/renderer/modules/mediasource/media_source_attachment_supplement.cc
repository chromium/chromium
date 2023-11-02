// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"

namespace blink {

MediaSourceAttachmentSupplement::MediaSourceAttachmentSupplement() = default;

MediaSourceAttachmentSupplement::~MediaSourceAttachmentSupplement() = default;

void MediaSourceAttachmentSupplement::AddMainThreadAudioTrackToMediaElement(
    String /* id */,
    String /* kind */,
    String /* label */,
    String /* language */,
    bool /* enabled */) {
  // TODO(https::/crbug.com/878133): Remove this once cross-thread
  // implementation supports creation of worker-thread tracks.
  NOTIMPLEMENTED();
}

void MediaSourceAttachmentSupplement::AddMainThreadVideoTrackToMediaElement(
    String /* id */,
    String /* kind */,
    String /* label */,
    String /* language */,
    bool /* selected */) {
  // TODO(https::/crbug.com/878133): Remove this once cross-thread
  // implementation supports creation of worker-thread tracks.
  NOTIMPLEMENTED();
}

bool MediaSourceAttachmentSupplement::RunExclusively(
    bool /* abort_if_not_fully_attached */,
    RunExclusivelyCB cb) {
  std::move(cb).Run(ExclusiveKey());
  return true;  // Indicates that we ran |cb|.
}

bool MediaSourceAttachmentSupplement::FullyAttachedOrSameThread(
    SourceBufferPassKey) const {
  return true;
}

void MediaSourceAttachmentSupplement::
    AssertCrossThreadMutexIsAcquiredForDebugging() {
  DCHECK(false)
      << "This should only be called on a CrossThreadMediaSourceAttachment";
}

void MediaSourceAttachmentSupplement::SendUpdatedInfoToMainThreadCache() {
  // No-op for the default implementation that is used by same-thread
  // attachments. Cross-thread attachments will override this. Same-thread
  // attachments will just directly calculate buffered and seekable when the
  // media element needs that info.
}

// protected
MediaSourceAttachmentSupplement::ExclusiveKey
MediaSourceAttachmentSupplement::GetExclusiveKey() const {
  return ExclusiveKey();
}

}  // namespace blink
