// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"
#import "ios/chrome/browser/file_upload_panel/ui/constants.h"
#import "ios/chrome/browser/file_upload_panel/ui/context_menu_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface FileUploadPanelCoordinator () <
    UIContextMenuInteractionDelegate,
    UINavigationControllerDelegate,
    UIImagePickerControllerDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation FileUploadPanelCoordinator {
  FileUploadPanelMediator* _mediator;
  ContextMenuPresenter* _contextMenuPresenter;
  UIImagePickerController* _cameraPicker;
}

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(activeWebState);
  CHECK(!activeWebState->IsBeingDestroyed());
  ChooseFileTabHelper* chooseFileTabHelper =
      ChooseFileTabHelper::FromWebState(activeWebState);
  CHECK(chooseFileTabHelper);
  ChooseFileController* chooseFileController =
      chooseFileTabHelper->GetChooseFileController();
  CHECK(chooseFileController);
  _mediator = [[FileUploadPanelMediator alloc]
      initWithChooseFileController:chooseFileController];
  _mediator.fileUploadPanelHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), FileUploadPanelCommands);

  if (_mediator.shouldShowCamera) {
    base::UmaHistogramEnumeration("IOS.FileUploadPanel.EntryPointVariant",
                                  FileUploadPanelEntryPointVariant::kCamera);
    [_mediator adjustCaptureTypeToAvailableDevices];
    [self showCamera];
    return;
  }

  if (_mediator.allowsDirectorySelection || !_mediator.allowsMediaSelection) {
    base::UmaHistogramEnumeration(
        "IOS.FileUploadPanel.EntryPointVariant",
        FileUploadPanelEntryPointVariant::kFilePicker);
    [self showFilePicker];
    return;
  }

  base::UmaHistogramEnumeration("IOS.FileUploadPanel.EntryPointVariant",
                                FileUploadPanelEntryPointVariant::kContextMenu);
  [self showContextMenu];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [self hideCamera];
  [self hideContextMenu];
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  __weak __typeof(self) weakSelf = self;
  UIContextMenuActionProvider actionProvider =
      ^UIMenu*(NSArray<UIMenuElement*>*) {
        return [weakSelf contextMenu];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  __weak __typeof(self) weakSelf = self;
  [animator addCompletion:^{
    [weakSelf doContextMenuInteractionEndAnimationCompletion];
  }];
}

#pragma mark - Private (Context Menu)

// Shows a context menu at the location of the last user interaction in the page
// with different options for file selection.
- (void)showContextMenu {
  if (_contextMenuPresenter) {
    return;
  }
  _contextMenuPresenter = [[ContextMenuPresenter alloc]
      initWithRootView:self.baseViewController.view];
  _contextMenuPresenter.contextMenuInteractionDelegate = self;
  [_contextMenuPresenter
      presentAtLocationInRootView:_mediator.eventScreenLocation];
}

// Returns the context menu to be presented by `-showContextMenu`.
- (UIMenu*)contextMenu {
  NSArray<UIMenuElement*>* actions = nil;
  __weak __typeof(self) weakSelf = self;

  UIAction* filePickerAction = [UIAction
      actionWithTitle:[self filePickerActionLabel]
                image:DefaultSymbolWithConfiguration(kFolderSymbol, nil)
           identifier:@"chromium.uploadfile.choosefile"
              handler:^(UIAction* action) {
                [weakSelf
                    showPickerForContextMenuActionVariant:
                        FileUploadPanelContextMenuActionVariant::kFilePicker];
              }];

  UIAction* photoPickerAction = [UIAction
      actionWithTitle:[self photoPickerActionLabel]
                image:DefaultSymbolWithConfiguration(kPhotoOnRectangleSymbol,
                                                     nil)
           identifier:@"chromium.uploadfile.choosephoto"
              handler:^(UIAction* action) {
                [weakSelf
                    showPickerForContextMenuActionVariant:
                        FileUploadPanelContextMenuActionVariant::kPhotoPicker];
              }];

  if ([UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    UIAction* cameraAction = [UIAction
        actionWithTitle:[self cameraActionLabel]
                  image:DefaultSymbolWithConfiguration(kSystemCameraSymbol, nil)
             identifier:@"chromium.uploadfile.usecamera"
                handler:^(UIAction* action) {
                  [weakSelf
                      showPickerForContextMenuActionVariant:
                          FileUploadPanelContextMenuActionVariant::kCamera];
                }];
    actions = @[ photoPickerAction, cameraAction, filePickerAction ];

    base::UmaHistogramEnumeration(
        "IOS.FileUploadPanel.ContextMenuVariant",
        FileUploadPanelContextMenuVariant::kPhotoPickerAndCameraAndFilePicker);
  } else {
    actions = @[ photoPickerAction, filePickerAction ];
    base::UmaHistogramEnumeration(
        "IOS.FileUploadPanel.ContextMenuVariant",
        FileUploadPanelContextMenuVariant::kPhotoPickerAndFilePicker);
  }

  return [UIMenu menuWithTitle:@"" children:actions];
}

// Hides the context menu presented by `-showContextMenu`.
- (void)hideContextMenu {
  [_contextMenuPresenter dismiss];
  _contextMenuPresenter = nil;
}

- (void)doContextMenuInteractionEndAnimationCompletion {
  [self hideContextMenu];
  if (!_cameraPicker) {
    [_mediator cancelFileSelection];
  }
}

#pragma mark - Private (File Picker)

// Returns the label to use for the file picker action in the context menu.
- (NSString*)filePickerActionLabel {
  // TODO(crbug.com/441659098): Use a plural label if multiple files can be
  // selected.
  return l10n_util::GetNSString(
      IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL);
}

// Shows a file picker to select one or several files on the device.
- (void)showFilePicker {
  // TODO(crbug.com/441659098): Show a file picker.
  [_mediator cancelFileSelection];
}

#pragma mark - Private (Photo Picker)

- (NSString*)photoPickerActionLabel {
  return l10n_util::GetNSString(
      IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL);
}

// Shows a photo picker to select one or several photos/videos on the device.
- (void)showPhotoPicker {
  // TODO(crbug.com/441659098): Show a photo picker.
  [_mediator cancelFileSelection];
}

#pragma mark - Private (Camera)

- (NSString*)cameraActionLabel {
  CHECK(_mediator.allowsMediaSelection);
  if (_mediator.allowsImageSelection && _mediator.allowsVideoSelection) {
    base::UmaHistogramEnumeration(
        "IOS.FileUploadPanel.CameraActionVariant",
        FileUploadPanelCameraActionVariant::kPhotoAndVideo);
    return l10n_util::GetNSString(
        IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_OR_VIDEO_ACTION_LABEL);
  }
  if (_mediator.allowsVideoSelection) {
    base::UmaHistogramEnumeration("IOS.FileUploadPanel.CameraActionVariant",
                                  FileUploadPanelCameraActionVariant::kVideo);
    return l10n_util::GetNSString(
        IDS_IOS_FILE_UPLOAD_PANEL_TAKE_VIDEO_ACTION_LABEL);
  }
  base::UmaHistogramEnumeration("IOS.FileUploadPanel.CameraActionVariant",
                                FileUploadPanelCameraActionVariant::kPhoto);
  return l10n_util::GetNSString(
      IDS_IOS_FILE_UPLOAD_PANEL_TAKE_PHOTO_ACTION_LABEL);
}

// Shows a camera view to take a photo/video to submit to the web page.
- (void)showCamera {
  CHECK([UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]);
  _cameraPicker = [[UIImagePickerController alloc] init];
  _cameraPicker.sourceType = UIImagePickerControllerSourceTypeCamera;
  _cameraPicker.mediaTypes = _mediator.acceptedMediaTypesAvailableForCamera;
  _cameraPicker.delegate = self;
  _cameraPicker.modalPresentationStyle = UIModalPresentationOverFullScreen;
  _cameraPicker.presentationController.delegate = self;
  if (_mediator.eventCaptureType != ChooseFileCaptureType::kNone) {
    _cameraPicker.cameraDevice = _mediator.preferredCameraDevice;
  }
  [self.baseViewController presentViewController:_cameraPicker
                                        animated:YES
                                      completion:nil];
}

- (void)hideCamera {
  [_cameraPicker.presentingViewController dismissViewControllerAnimated:YES
                                                             completion:nil];
  _cameraPicker = nil;
}

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:
        (NSDictionary<UIImagePickerControllerInfoKey, id>*)info {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.CameraResult", true);
  [_mediator submitFileSelectionWithMediaInfo:info];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.CameraResult", false);
  [_mediator cancelFileSelection];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_mediator cancelFileSelection];
}

#pragma mark - Private

- (void)showPickerForContextMenuActionVariant:
    (FileUploadPanelContextMenuActionVariant)actionVariant {
  base::UmaHistogramEnumeration("IOS.FileUploadPanel.ContextMenuActionVariant",
                                actionVariant);
  switch (actionVariant) {
    case FileUploadPanelContextMenuActionVariant::kFilePicker:
      [self showFilePicker];
      break;
    case FileUploadPanelContextMenuActionVariant::kPhotoPicker:
      [self showPhotoPicker];
      break;
    case FileUploadPanelContextMenuActionVariant::kCamera:
      [self showCamera];
      break;
  }
}

@end
