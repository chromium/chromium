// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_

#import <Foundation/Foundation.h>

// The fallback height for the minimized detent.
extern const NSInteger kAssistantContainerMinimizedDetentHeight;

// Represents a detent of the Assistant Container.
// Backed by NSInteger and explicitly ordered by increasing height.
// This allows the raw values to be used directly for sorting detent arrays.
enum class AssistantContainerDetent : NSInteger {
  // Minimized detent (fixed height).
  kMinimized = 0,
  // Medium detent (50%).
  kMedium,
  // Large detent (100%).
  kLarge,
};

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_
