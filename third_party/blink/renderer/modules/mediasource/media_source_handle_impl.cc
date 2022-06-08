// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

MediaSourceHandleImpl::MediaSourceHandleImpl(
    scoped_refptr<MediaSourceAttachment> attachment,
    String internal_blob_url)
    : attachment_(attachment), internal_blob_url_(internal_blob_url) {
  DCHECK(attachment_);
  DCHECK(!internal_blob_url.IsEmpty());

  DVLOG(1) << __func__ << " this=" << this << ", attachment_=" << attachment_
           << ", internal_blob_url_=" << internal_blob_url_;
}

MediaSourceHandleImpl::~MediaSourceHandleImpl() {
  DVLOG(1) << __func__ << " this=" << this;
}

scoped_refptr<MediaSourceAttachment> MediaSourceHandleImpl::GetAttachment() {
  return attachment_;
}

String MediaSourceHandleImpl::GetInternalBlobURL() {
  return internal_blob_url_;
}

void MediaSourceHandleImpl::mark_serialized() {
  DCHECK(!serialized_);
  serialized_ = true;
}

void MediaSourceHandleImpl::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
