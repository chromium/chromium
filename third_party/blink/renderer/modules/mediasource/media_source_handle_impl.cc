// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/handle_attachment_provider.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

MediaSourceHandleImpl::MediaSourceHandleImpl(
    scoped_refptr<HandleAttachmentProvider> attachment_provider,
    String internal_blob_url)
    : attachment_provider_(std::move(attachment_provider)),
      internal_blob_url_(internal_blob_url) {
  DCHECK(attachment_provider_);
  DCHECK(!internal_blob_url.empty());

  DVLOG(1) << __func__ << " this=" << this
           << ", attachment_provider_=" << attachment_provider_
           << ", internal_blob_url_=" << internal_blob_url_;
}

MediaSourceHandleImpl::~MediaSourceHandleImpl() {
  DVLOG(1) << __func__ << " this=" << this;
}

scoped_refptr<HandleAttachmentProvider>
MediaSourceHandleImpl::TakeAttachmentProvider() {
  DVLOG(1) << __func__ << " this=" << this;
  return std::move(attachment_provider_);
}

scoped_refptr<MediaSourceAttachment> MediaSourceHandleImpl::TakeAttachment() {
  DVLOG(1) << __func__ << " this=" << this;
  if (!attachment_provider_) {
    // Either this handle instance has already been serialized, has been
    // transferred, or it has been assigned as srcObject on an HTMLMediaElement
    // and used later to begin asynchronous attachment start.
    DCHECK(is_serialized() || detached_ || is_used());
    return nullptr;
  }

  // Otherwise, this handle instance must not yet have been serialized,
  // transferred or used to begin an attachment. The only case we should be here
  // is when this instance is being used to attempt asynchronous attachment
  // start after it was set as srcObject on an HTMLMediaElement.
  DCHECK(is_used() && !is_serialized() && !detached_);
  scoped_refptr<MediaSourceAttachment> result =
      attachment_provider_->TakeAttachment();
  attachment_provider_ = nullptr;

  // Note that |result| would be nullptr here if some other duplicated handle
  // (due to postMessage's lack of true move-only semantics) has already started
  // asynchronous attachment for the same underlying attachment (and
  // MediaSource).
  return result;
}

String MediaSourceHandleImpl::GetInternalBlobURL() {
  DVLOG(1) << __func__ << " this=" << this;
  return internal_blob_url_;
}

void MediaSourceHandleImpl::mark_serialized() {
  DVLOG(1) << __func__ << " this=" << this;
  DCHECK(!serialized_);
  serialized_ = true;

  // Before being serialized, the serialization must have retrieved our
  // reference to the |attachment_provider_| precisely once. Note that
  // immediately upon an instance of us being assigned to srcObject, that
  // instance can no longer be serialized and there will be at most one async
  // media element load that retrieves our provider's attachment reference.
  DCHECK(!attachment_provider_);
}

void MediaSourceHandleImpl::mark_detached() {
  DVLOG(1) << __func__ << " this=" << this;
  DCHECK(!detached_);
  detached_ = true;
  attachment_provider_ = nullptr;
}

void MediaSourceHandleImpl::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
