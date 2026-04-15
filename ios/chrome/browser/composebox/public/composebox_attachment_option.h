// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_

// Enum representing the different attachment options in the composebox.
enum class ComposeboxAttachmentOption {
  // Attaching the current active tab.
  kCurrentTab,
  // Attaching a tab.
  kTab,
  // Attaching a file.
  kFile,
  // Attaching an image from the gallery.
  kGallery,
  // Attaching a photo from the camera.
  kCamera,
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_
