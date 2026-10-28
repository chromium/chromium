// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_DATA_PROTECTION_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_DATA_PROTECTION_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// Maximum number of tabs in the WebStateList to inspect individually when
// checking if any tab requires watermarking in the Tab Grid. 99.9% of users
// have fewer than 300 active tabs, so set 600 as the limit to avoid
// performance degradation.
inline constexpr int kMaxTabsToCheckForWatermark = 600;

// Scene agent that manages screenshot protection for the scene's window.
// It obfuscates the window when the Tab Grid is visible and screenshot
// protection policies are enabled, or when the active tab requires protection.
@interface DataProtectionSceneAgent : ObservingSceneAgent
@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_COORDINATOR_DATA_PROTECTION_SCENE_AGENT_H_
