// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_

class Browser;
struct UrlLoadParams;
@class TabGridCoordinator;
@protocol BrowserInterface;

// Methods exposed for testing. This is terrible and should be rewritten.
@interface SceneController ()

@property(nonatomic, strong) TabGridCoordinator* mainCoordinator;

- (void)showLegacyFirstRunUI;

- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams;

// Dismisses all modal dialogs, excluding the omnibox if `dismissOmnibox` is
// NO, then call `completion`.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

- (id<BrowserInterface>)currentInterface;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_TESTING_H_
