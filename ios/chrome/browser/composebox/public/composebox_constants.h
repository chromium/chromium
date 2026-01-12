// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_CONSTANTS_H_

namespace composeboxAttachments {

/// The height of input items.
const float kAttachmentHeight = 44.0f;
/// The corner radius of input items.
const float kAttachmentCornerRadius = kAttachmentHeight / 2;
/// Image input item size.
const CGSize kImageInputItemSize = {86.0f, kAttachmentHeight};
/// Tab/File input item size.
const CGSize kTabFileInputItemSize = {136.0f, kAttachmentHeight};

}  // namespace composeboxAttachments

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_CONSTANTS_H_
