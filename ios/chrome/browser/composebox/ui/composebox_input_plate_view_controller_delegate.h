// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_DELEGATE_H_

#import <vector>

enum class AiModeActivationSource;
enum class ComposeboxDragAndDropType;
@class ComposeboxInputPlateViewController;
enum class ComposeboxMode;
enum class ComposeboxModelOption;
@class ComposeboxUIInputState;
enum class FuseboxAttachmentButtonType;

/// Delegate for the composebox input plate view controller.
@protocol ComposeboxInputPlateViewControllerDelegate

/// Informs the delegate that the input plate completed the initial
/// presentation.
- (void)composeboxViewControllerDidCompleteInitialPresentation:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that a user did tap on the gallery button.
- (void)composeboxViewControllerDidTapGalleryButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that a user did tap on the mic button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                 didTapMicButton:(UIButton*)button;

/// Informs the delegate that a user did tap on the lens button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapLensButton:(UIButton*)button;

/// Informs the delegate that a user tapped on the QR scanner button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
           didTapQRScannerButton:(UIButton*)button;

/// Informs the delegate that a user did tap on the camera button.
- (void)composeboxViewControllerDidTapCameraButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that the plus menu opened and passes the visible
/// attachment buttons.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
    didOpenPlusMenuWithVisibleInternalButtons:
        (const std::vector<FuseboxAttachmentButtonType>&)visibleInternalButtons
                                 uiInputState:(ComposeboxUIInputState*)state;

/// Informs the delegate that a user did tap on the file button.
- (void)composeboxViewControllerDidTapFileButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that a user did tap on the drive button.
- (void)composeboxViewControllerDidTapDriveButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that a user did tap on the attach tabs button.
- (void)composeboxViewControllerDidTapAttachTabsButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Informs the delegate that a drag and drop was attempted.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)viewController
       didAttemptDragAndDropType:(ComposeboxDragAndDropType)type;

/// Informs the delegate that a user did tap on a tool button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                      didTapTool:(ComposeboxMode)toolMode
                activationSource:(AiModeActivationSource)activationSource;

/// Informs the delegate that a user did select a model option from the
/// tool menu.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                  didSelectModel:(ComposeboxModelOption)modelOption;

/// Informs the delegate that a user did tap on the plus button.
- (void)composeboxViewControllerDidTapPlusButton:
            (ComposeboxInputPlateViewController*)composeboxViewController
                                withUIInputState:(ComposeboxUIInputState*)state;

/// Informs the delegate that a user did tap on the lens button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapSendButton:(UIButton*)button;

/// Informs the delegate that a file attachment did fail due to exceeding the
/// attachment limit.
- (void)didFailToAttachDueToIneligibleAttachments:
    (ComposeboxInputPlateViewController*)composeboxViewController;

/// Returns whether the given `tabInfo` is present on the current profile, with
/// the same off-the-record status.
- (BOOL)tabExistsOnCurrentProfile:(TabInfo*)tabInfo;

/// Returns the web state associated with a given `tabInfo` on the current
/// profile.  Returns `nullptr` if none is found.
- (web::WebState*)webStateForTabOnCurrentProfile:(TabInfo*)tabInfo;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_DELEGATE_H_
