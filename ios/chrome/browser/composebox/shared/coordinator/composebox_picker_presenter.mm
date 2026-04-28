// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <PhotosUI/PhotosUI.h>

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"

@interface ComposeboxPickerPresenter () <PHPickerViewControllerDelegate,
                                         UINavigationControllerDelegate,
                                         UIImagePickerControllerDelegate>

@end

@implementation ComposeboxPickerPresenter {
  // The VC used as a base for presentations.
  __weak UIViewController* _baseViewController;
}

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
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

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:(NSDictionary<NSString*, id>*)info {
  UIImage* image = info[UIImagePickerControllerOriginalImage];
  if (!image) {
    [picker dismissViewControllerAnimated:YES completion:nil];
    return;
  }

  NSItemProvider* provider = [[NSItemProvider alloc] initWithObject:image];
  [self.delegate
      composeboxPickerPresenter:self
                  didPickImages:@[ [[ComposeboxPickerImageResult alloc]
                                    initWithImageProvider:provider
                                                  assetID:nil] ]];
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
                                            assetID:result.assetIdentifier]];
  }

  [self.delegate composeboxPickerPresenter:self didPickImages:imageItems];
}

@end
