// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_

#include "base/containers/enum_set.h"

// Enum representing the different attachment options in the composebox.
enum class ComposeboxAttachmentOption {
  // Attaching the current active tab.
  kCurrentTab = 0,
  // Attaching a tab.
  kTab = 1,
  // Attaching a file.
  kFile = 2,
  // Attaching an image from the gallery.
  kGallery = 3,
  // Attaching a photo from the camera.
  kCamera = 4,
  // The maximum value for iteration.
  kMaxValue = kCamera,
};

// A set of ComposeboxAttachmentOption values used for iteration.
using ComposeboxAttachmentOptionSet =
    base::EnumSet<ComposeboxAttachmentOption,
                  ComposeboxAttachmentOption::kCurrentTab,
                  ComposeboxAttachmentOption::kMaxValue>;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_OPTION_H_
