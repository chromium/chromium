// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/old_document_info_for_commit.h"

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

OldDocumentInfoForCommit::OldDocumentInfoForCommit(
    scoped_refptr<SecurityOrigin> new_document_origin)
    : unload_timing_info(
          UnloadEventTimingInfo(std::move(new_document_origin))) {}

void OldDocumentInfoForCommit::Trace(Visitor* visitor) const {
  visitor->Trace(history_item);
}

ScopedOldDocumentInfoForCommitCapturer*
    ScopedOldDocumentInfoForCommitCapturer::current_capturer_ = nullptr;

ScopedOldDocumentInfoForCommitCapturer::
    ~ScopedOldDocumentInfoForCommitCapturer() {
  current_capturer_ = previous_capturer_;
}

}  // namespace blink
