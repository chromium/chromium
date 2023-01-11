// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_
#define MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"

namespace media {

// This is soon to be deprecated in favor of VideoFrame destruction CBs.
// Please do not use this.

namespace deprecated {
// Similar to VideoFrame::ReleaseMailboxCB for now.
using ReleaseMailboxCB = base::OnceCallback<void(const gpu::SyncToken&)>;
using OutputWithReleaseMailboxCB =
    base::RepeatingCallback<void(ReleaseMailboxCB, scoped_refptr<VideoFrame>)>;
}  // namespace deprecated

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_
