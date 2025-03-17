// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// The maximum length of string as data url.
extern const size_t kMaxLengthOfDataURLString;

// Accept header used for frame requests.
CONTENT_EXPORT extern const char kFrameAcceptHeaderValue[];

// Constants for attaching message pipes to the mojo invitation used to
// initialize child processes.
extern const int kChildProcessReceiverAttachmentName;
extern const int kChildProcessHostRemoteAttachmentName;
extern const int kLegacyIpcBootstrapAttachmentName;

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
