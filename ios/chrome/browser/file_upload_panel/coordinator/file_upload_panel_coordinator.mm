// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"

#import <PhotosUI/PhotosUI.h>
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
    UIDocumentPickerDelegate,
    PHPickerViewControllerDelegate,
    UIImagePickerControllerDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation FileUploadPanelCoordinator {
  FileUploadPanelMediator* _mediator;
  ContextMenuPresenter* _contextMenuPresenter;
  UIImagePickerController* _cameraPicker;
  UIDocumentPickerViewController* _filePicker;
  PHPickerViewController* _photoPicker;
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
  [self hideFilePicker];
  [self hidePhotoPicker];
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
  if (!_cameraPicker && !_filePicker && !_photoPicker) {
    [_mediator cancelFileSelection];
  }
}

#pragma mark - Private (File Picker)

// Returns the label to use for the file picker action in the context menu.
- (NSString*)filePickerActionLabel {
  if (_mediator.allowsMultipleSelection) {
    return l10n_util::GetNSString(
        IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILES_ACTION_LABEL);
  }
  return l10n_util::GetNSString(
      IDS_IOS_FILE_UPLOAD_PANEL_CHOOSE_FILE_ACTION_LABEL);
}

// Shows a file picker to select one or several files on the device.
- (void)showFilePicker {
  _filePicker = [[UIDocumentPickerViewController alloc]
      initForOpeningContentTypes:_mediator.acceptedDocumentTypes
                          asCopy:!_mediator.allowsDirectorySelection];
  _filePicker.allowsMultipleSelection = _mediator.allowsMultipleSelection;
  _filePicker.delegate = self;
  _filePicker.presentationController.delegate = self;
  [self.baseViewController presentViewController:_filePicker
                                        animated:YES
                                      completion:nil];
}

- (void)hideFilePicker {
  [_filePicker.presentingViewController dismissViewControllerAnimated:YES
                                                           completion:nil];
  _filePicker = nil;
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.FilePicker.Result", true);
  base::UmaHistogramCounts100("IOS.FileUploadPanel.FilePicker.FileCount",
                              urls.count);
  NSURL* securityScopedResource = nil;
  if (_mediator.allowsDirectorySelection) {
    CHECK_EQ(urls.count, 1u);
    securityScopedResource = urls.firstObject;
    if (![securityScopedResource startAccessingSecurityScopedResource]) {
      // If access to a security scoped resource was required but could not be
      // granted, cancelling file selection.
      base::UmaHistogramEnumeration(
          "IOS.FileUploadPanel.SecurityScopedResource.AccessState",
          FileUploadPanelSecurityScopedResourceAccessState::kStartFailed);
      [_mediator cancelFileSelection];
      return;
    }
  }
  [_mediator submitFileSelection:urls];
  // After submitting selection, the coordinator may have stopped and the
  // mediator may have been disconnected. Access to security scoped resources
  // should still be stopped if necessary.
  [securityScopedResource stopAccessingSecurityScopedResource];
  if (securityScopedResource) {
    base::UmaHistogramEnumeration(
        "IOS.FileUploadPanel.SecurityScopedResource.AccessState",
        FileUploadPanelSecurityScopedResourceAccessState::kStartedAndStopped);
  }
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.FilePicker.Result", false);
  [_mediator cancelFileSelection];
}

#pragma mark - Private (Photo Picker)

- (NSString*)photoPickerActionLabel {
  return l10n_util::GetNSString(
      IDS_IOS_FILE_UPLOAD_PANEL_PHOTO_LIBRARY_ACTION_LABEL);
}

// Shows a photo picker to select one or several photos/videos on the device.
- (void)showPhotoPicker {
  PHPickerConfiguration* configuration = [[PHPickerConfiguration alloc] init];
  configuration.selectionLimit = _mediator.allowsMultipleSelection ? 0 : 1;
  configuration.preferredAssetRepresentationMode =
      PHPickerConfigurationAssetRepresentationModeCurrent;
  if (_mediator.allowsImageSelection && !_mediator.allowsVideoSelection) {
    configuration.filter = PHPickerFilter.imagesFilter;
  } else if (_mediator.allowsVideoSelection &&
             !_mediator.allowsImageSelection) {
    configuration.filter = PHPickerFilter.videosFilter;
  }

  _photoPicker =
      [[PHPickerViewController alloc] initWithConfiguration:configuration];
  _photoPicker.delegate = self;
  _photoPicker.presentationController.delegate = self;
  [self.baseViewController presentViewController:_photoPicker
                                        animated:YES
                                      completion:nil];
}

- (void)hidePhotoPicker {
  [_photoPicker.presentingViewController dismissViewControllerAnimated:YES
                                                            completion:nil];
  _photoPicker = nil;
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.PhotoPicker.Result",
                            results.count > 0);
  if (results.count == 0) {
    [_mediator cancelFileSelection];
  } else {
    base::UmaHistogramCounts100("IOS.FileUploadPanel.PhotoPicker.FileCount",
                                results.count);
    [_mediator submitFileSelectionWithPickerResults:results];
  }
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
  base::UmaHistogramBoolean("IOS.FileUploadPanel.Camera.Result", true);
  [_mediator submitFileSelectionWithMediaInfo:info];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  base::UmaHistogramBoolean("IOS.FileUploadPanel.Camera.Result", false);
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
