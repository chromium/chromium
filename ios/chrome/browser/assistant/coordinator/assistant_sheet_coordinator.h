// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Modes for the assistant sheet.
typedef NS_ENUM(NSUInteger, AssistantSheetMode) {
  // Mode for the AI assistant.
  AssistantSheetModeAI,
  // Mode for Gemini.
  AssistantSheetModeGemini,
};

// Coordinator for the assistant sheet.
@interface AssistantSheetCoordinator : ChromeCoordinator

// The mode of the assistant sheet.
@property(nonatomic, assign) AssistantSheetMode mode;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_COORDINATOR_H_
