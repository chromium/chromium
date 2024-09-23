// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_

class Browser;
struct UrlLoadParams;
@class TabGridCoordinator;
@class UserFeedbackData;
@class WrangledBrowser;

using UserFeedbackDataCallback =
    base::RepeatingCallback<void(UserFeedbackData*)>;

// Methods exposed for testing. This is terrible and should be rewritten.
@interface SceneController ()

@property(nonatomic, strong) TabGridCoordinator* mainCoordinator;

- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams;

- (void)presentSignInAccountsViewControllerIfNecessary;

// Dismisses all modal dialogs, excluding the omnibox if `dismissOmnibox` is
// NO, then call `completion`.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Presents the "Report an issue" screen within the given `timeout`.
- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                          userFeedbackData:(UserFeedbackData*)userFeedbackData
                                   timeout:(base::TimeDelta)timeout
                                completion:(UserFeedbackDataCallback)completion;

- (WrangledBrowser*)currentInterface;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_
