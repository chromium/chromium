// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/byte_size.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"

namespace content {

// The maximum length of string as data url.
inline constexpr base::ByteSize kMaxLengthOfDataURLString = base::MiBU(20);

// Accept header used for frame requests.
// Note: JXL inclusion is determined at runtime via features::kJXLImageFormat.
// These constants provide the base values with and without JXL.
#if BUILDFLAG(ENABLE_AV1_DECODER) && BUILDFLAG(ENABLE_JXL_DECODER)
inline constexpr char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
    "image/webp,image/apng,*/*;q=0.8";
inline constexpr char kFrameAcceptHeaderValueWithJxl[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/jxl,"
    "image/avif,image/webp,image/apng,*/*;q=0.8";
#elif BUILDFLAG(ENABLE_AV1_DECODER)
inline constexpr char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
    "image/webp,image/apng,*/*;q=0.8";
#elif BUILDFLAG(ENABLE_JXL_DECODER)
inline constexpr char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
    "image/apng,*/*;q=0.8";
inline constexpr char kFrameAcceptHeaderValueWithJxl[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/jxl,"
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
