// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The different modes for the composebox.
enum class ComposeboxMode {
  // Performs a regular search.
  kRegularSearch,
  // Performs an AI Mode search.
  kAIM,
  // Creates an image based on the input.
  kImageGeneration,
  // Generates a new canvas based on the input query.
  kCanvas,
  // Helps user with complex research tasks.
  kDeepSearch,
};

// The maximum number of attachments that can be added to a prompt.
extern const NSUInteger kAttachmentLimit;

// The maximum number of attachments that can be added to a prompt.
extern const NSUInteger kAttachmentLimitForImageGeneration;

// The maximum allowed size for PDF file uploads.
extern const NSUInteger kMaxPDFFileSize;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_CONSTANTS_H_
