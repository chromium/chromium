// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTATION_CONTEXT_H_

#import <Foundation/Foundation.h>

// Describes the presentation context of the Assistant Container.
enum class AssistantPresentationContext {
  // Standard compact-width presentation (e.g., iPhone portrait).
  // The container behaves as a traditional bottom sheet.
  kSheet,
  // Regular-width presentation (e.g., iPad full screen).
  // The container is presented as a side panel.
  kPanel,
};

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTATION_CONTEXT_H_
