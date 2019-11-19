// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FTP_DIRECTORY_LISTING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FTP_DIRECTORY_LISTING_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class KURL;

// Translates |input|, an FTP LISTING result, to an HTML and returns it. When
// an error happens that is written in the result HTML.
PLATFORM_EXPORT scoped_refptr<SharedBuffer> GenerateFtpDirectoryListingHtml(
    const KURL& url,
    const SharedBuffer* input);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FTP_DIRECTORY_LISTING_H_
