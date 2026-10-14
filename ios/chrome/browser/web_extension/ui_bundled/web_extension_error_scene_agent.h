// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_UI_BUNDLED_WEB_EXTENSION_ERROR_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_UI_BUNDLED_WEB_EXTENSION_ERROR_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// A scene agent that monitors web extension load errors and presents a snackbar
// allowing the user to navigate to Universal Opt Out settings when an error
// occurs either at startup or dynamically while using the browser.
@interface WebExtensionErrorSceneAgent : ObservingSceneAgent
@end

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_UI_BUNDLED_WEB_EXTENSION_ERROR_SCENE_AGENT_H_
