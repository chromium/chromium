// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_ACCEPT_HEADER_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_ACCEPT_HEADER_H_

#include <string>

#include "content/common/content_export.h"

namespace content {
class BrowserContext;

// The value that should be set for the "Accept" header for frame navigations.
// Includes signed exchange and image decoder information (when applicable) that
// are not available from the network service. This may also accept signed
// exchange responses when |allow_sxg_responses| is true.
CONTENT_EXPORT std::string FrameAcceptHeaderValue(
    bool allow_sxg_responses,
    BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_ACCEPT_HEADER_H_
