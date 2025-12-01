// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The different modes for the composebox.
enum class ComposeboxMode {
  kRegularSearch,
  kAIM,
  kImageGeneration,
};

// The maximum number of attachments that can be added to a prompt.
extern const NSUInteger kAttachmentLimit;
// The maximum allowed size for PDF file uploads.
extern const NSUInteger kMaxPDFFileSize;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_
