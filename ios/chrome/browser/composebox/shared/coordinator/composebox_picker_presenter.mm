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

@interface ComposeboxPickerPresenter () <PHPickerViewControllerDelegate,
                                         UIDocumentPickerDelegate,
                                         UIImagePickerControllerDelegate,
                                         UINavigationControllerDelegate>
@end

@implementation ComposeboxPickerPresenter {
  // The VC used as a base for presentations.
  __weak UIViewController* _baseViewController;
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
  if (!_browser) {
    return;
  }

  [self createSnackbarPresenterIfNeeded];

  TabPickerParams* params =
      [[TabPickerParams alloc] initWithSnackbarPresenter:_snackbarPresenter];
  params.maxTabAttachmentCount =
      [self.dataSource maxTabAttachmentCountForPresenter:self];
  params.preselectedWebStateIDs =
      [self.dataSource attachedWebStateIDsInCurrentContextForPresenter:self];
  params.baseViewController = _baseViewController;

  __weak __typeof(self) weakSelf = self;
  TabPickerCompletionBlock completionBlock =
      ^(std::set<web::WebStateID> selectedIDs,
        std::set<web::WebStateID> cachedIDs) {
        [weakSelf.delegate composeboxPickerPresenter:weakSelf
                   handleSelectedTabsWithWebStateIDs:selectedIDs
                                   cachedWebStateIDs:cachedIDs];
      };

  id<TabPickerCommands> tabPickerHandler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), TabPickerCommands);
  [tabPickerHandler showTabPickerWithParams:params completion:completionBlock];
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

#pragma mark - Private

- (void)createSnackbarPresenterIfNeeded {
  if (_snackbarPresenter || !_browser) {
    return;
  }
  _snackbarPresenter =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:_browser.get()];
}

@end
