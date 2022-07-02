// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_

#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace blink {

// Whether or not a fenced frame is allowed to be navigated to `url`. For now
// this is described by
// https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md.
BLINK_COMMON_EXPORT bool IsValidFencedFrameURL(const GURL& url);

// Whether or not a URL is a valid "urn uuid URL" depends not only on just the
// scheme being "urn", but that the URL's prefix is "urn:uuid".
BLINK_COMMON_EXPORT bool IsValidUrnUuidURL(const GURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_FENCED_FRAME_UTILS_H_
