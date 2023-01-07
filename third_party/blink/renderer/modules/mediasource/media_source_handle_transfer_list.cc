// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_handle_transfer_list.h"

#include "base/logging.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"

namespace blink {

// static
const void* const MediaSourceHandleTransferList::kTransferListKey = nullptr;

MediaSourceHandleTransferList::MediaSourceHandleTransferList() = default;

MediaSourceHandleTransferList::~MediaSourceHandleTransferList() = default;

void MediaSourceHandleTransferList::FinalizeTransfer(ExceptionState&) {
  DVLOG(3) << __func__ << " this=" << this;
  for (MediaSourceHandleImpl* handle : media_source_handles) {
    handle->mark_detached();
  }
}

void MediaSourceHandleTransferList::Trace(Visitor* visitor) const {
  visitor->Trace(media_source_handles);
}

}  // namespace blink
