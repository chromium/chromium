// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/byte_count.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/media_buildflags.h"

namespace content {

// The maximum length of string as data url.
inline constexpr base::ByteCount kMaxLengthOfDataURLString = base::MiB(20);

// Accept header used for frame requests.
#if BUILDFLAG(ENABLE_AV1_DECODER)
inline constexpr char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
    "image/webp,image/apng,*/*;q=0.8";
#else
inline constexpr char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
    "image/apng,*/*;q=0.8";
#endif

// Constants for attaching message pipes to the mojo invitation used to
// initialize child processes.
inline constexpr int kChildProcessReceiverAttachmentName = 0;
inline constexpr int kChildProcessHostRemoteAttachmentName = 1;
inline constexpr int kLegacyIpcBootstrapAttachmentName = 2;

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
