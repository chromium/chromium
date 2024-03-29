// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/handle_attachment_provider.h"

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

HandleAttachmentProvider::HandleAttachmentProvider(
    scoped_refptr<MediaSourceAttachment> attachment)
    : attachment_(std::move(attachment)) {
  DCHECK(attachment_);
  DVLOG(1) << __func__ << " this=" << this << ", attachment_=" << attachment_;
}

HandleAttachmentProvider::~HandleAttachmentProvider() {
  DVLOG(1) << __func__ << " this=" << this;
}

scoped_refptr<MediaSourceAttachment>
HandleAttachmentProvider::TakeAttachment() {
  base::AutoLock locker(attachment_lock_);

  DVLOG(1) << __func__ << " this=" << this << ", attachment_=" << attachment_;
  return std::move(attachment_);
}

}  // namespace blink
