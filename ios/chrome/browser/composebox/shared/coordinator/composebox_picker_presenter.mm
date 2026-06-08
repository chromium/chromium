// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <PhotosUI/PhotosUI.h>

#import "base/memory/weak_ptr.h"
#import "components/lens/lens_features.h"
#import "ios/chrome/browser/composebox/public/composebox_input_item_source.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_snackbar_presenter.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_coordinator.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_logger.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_snackbar_presenter.h"

@interface ComposeboxPickerPresenter () <PHPickerViewControllerDelegate,
                                         UINavigationControllerDelegate,
                                         UIImagePickerControllerDelegate,
                                         UIDocumentPickerDelegate,
                                         TabPickerCommands,
                                         TabPickerSelectionDelegate>
@end

@implementation ComposeboxPickerPresenter {
  // The VC used as a base for presentations.
  __weak UIViewController* _baseViewController;
  // Coordinator for the tab picker.
  TabPickerCoordinator* _tabPickerCoordinator;
  base::WeakPtr<Browser> _browser;

  // Presents snackbars.
  ComposeboxSnackbarPresenter* _snackbarPresenter;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _browser = browser->AsWeakPtr();
  }

  return self;
}

- (void)presentCameraPicker {
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    // TODO(crbug.com/40280872): Show an error to the user.
    return;
  }

  UIImagePickerController* picker = [[UIImagePickerController alloc] init];
  picker.delegate = self;
  picker.sourceType = UIImagePickerControllerSourceTypeCamera;
  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentGalleryPickerWithLimit:(NSUInteger)limit {
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc]
      initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];
  config.selectionLimit = limit;
  config.filter = [PHPickerFilter imagesFilter];
  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:config];
  picker.delegate = self;

  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentFilePicker {
  UIDocumentPickerViewController* picker;
  if (lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[ UTTypeData ]];
  } else {
    picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[ UTTypePDF ]];
  }

  picker.allowsMultipleSelection = NO;
  picker.delegate = self;

  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentTabPicker {
  [self showTabPicker];
}

- (void)presentDriveFilePicker {
  id<DriveFilePickerCommands> driveFilePickerCommands = HandlerForProtocol(
      _browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [driveFilePickerCommands
      showDriveFilePickerWithComposeboxDelegate:self.delegate
                             baseViewController:_baseViewController];
}

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:(NSDictionary<NSString*, id>*)info {
  __weak __typeof(self) weakSelf = self;
  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];

  UIImage* image = info[UIImagePickerControllerOriginalImage];
  if (!image) {
    return;
  }

  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];

  NSItemProvider* provider = [[NSItemProvider alloc] initWithObject:image];
  [self.delegate
      composeboxPickerPresenter:self
                  didPickImages:@[
                    [[ComposeboxPickerImageResult alloc]
                        initWithImageProvider:provider
                                      assetID:nil
                                       source:ComposeboxInputItemSource::
                                                  kCameraPicker]
                  ]];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  __weak __typeof(self) weakSelf = self;
  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];
}

#pragma mark - TabPickerCommands

- (void)showTabPicker {
  if (!_browser) {
    return;
  }

  _tabPickerCoordinator = [[TabPickerCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser.get()];
  // TODO(crbug.com/40280872): Integrate logger and snackbar presenter
  [self createSnackbarPresenterIfNeeded];
  _tabPickerCoordinator.snackbarPresenter = _snackbarPresenter;
  _tabPickerCoordinator.delegate = self;
  _tabPickerCoordinator.tabPickerHandler = self;
  [_tabPickerCoordinator start];
}

- (void)hideTabPicker {
  [_tabPickerCoordinator stop];
  _tabPickerCoordinator = nil;
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  [picker dismissViewControllerAnimated:YES completion:nil];
  if (results.count == 0) {
    return;
  }

  // TODO(crbug.com/506955766): Unify metrics recording and record this action.

  NSMutableArray<ComposeboxPickerImageResult*>* imageItems =
      [[NSMutableArray alloc] initWithCapacity:results.count];
  for (PHPickerResult* result in results) {
    [imageItems addObject:[[ComposeboxPickerImageResult alloc]
                              initWithImageProvider:result.itemProvider
                                            assetID:result.assetIdentifier
                                             source:ComposeboxInputItemSource::
                                                        kGalleryPicker]];
  }

  [self.delegate composeboxPickerPresenter:self didPickImages:imageItems];
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  [self.delegate composeboxPickerPresenter:self didPickFilesWithURLs:urls];
}

// Returns the associated IDs for all currently attached tabs.
- (std::set<web::WebStateID>)allAttachedWebStateIDs {
  return [self.dataSource allAttachedWebStateIDsForPresenter:self];
}

// Returns the associated IDs for currently attached tabs from the current web
// state context. Tabs attached from different web states (not visible in the
// tab picker) will be excluded.
- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContext {
  return [self.dataSource attachedWebStateIDsInCurrentContextForPresenter:self];
}

// Returns the max number of tab attachments.
- (NSUInteger)maxTabAttachmentCount {
  return [self.dataSource maxTabAttachmentCountForPresenter:self];
}

// Attaches the selected tabs. `cachedWebStateIDs` contains the IDs of the
// tabs that have their content cached.
- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs {
  [self.delegate composeboxPickerPresenter:self
         handleSelectedTabsWithWebStateIDs:selectedWebStateIDs
                         cachedWebStateIDs:cachedWebStateIDs];
}

#pragma mark - Private

- (void)createSnackbarPresenterIfNeeded {
  if (_snackbarPresenter || !_browser) {
    return;
  }
  _snackbarPresenter =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:_browser.get()];
}

@end
