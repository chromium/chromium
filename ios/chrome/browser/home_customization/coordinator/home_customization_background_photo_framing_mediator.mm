// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_framing_mediator.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/functional/callback_forward.h"
#import "base/logging.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_mutator.h"

@implementation HomeCustomizationBackgroundPhotoFramingMediator {
  raw_ptr<UserUploadedImageManager> _userUploadedImageManager;
  raw_ptr<HomeBackgroundCustomizationService> _backgroundService;
}

- (instancetype)initWithUserUploadedImageManager:
                    (UserUploadedImageManager*)userUploadedImageManager
                               backgroundService:
                                   (HomeBackgroundCustomizationService*)
                                       backgroundService {
  self = [super init];
  if (self) {
    _userUploadedImageManager = userUploadedImageManager;
    _backgroundService = backgroundService;
  }
  return self;
}

- (void)discardBackground {
  _backgroundService->RestoreCurrentTheme();
}

#pragma mark - HomeCustomizationBackgroundPhotoFramingMutator

- (void)saveImage:(UIImage*)image
    withFramingCoordinates:(HomeCustomizationFramingCoordinates*)coordinates
                completion:(base::OnceClosure)completion {
  DCHECK(image);
  DCHECK(coordinates);

  __weak __typeof(self) weakSelf = self;
  _userUploadedImageManager->StoreUserUploadedImage(
      image, base::BindOnce(
                 ^(base::OnceClosure finalCompletion, base::FilePath path) {
                   [weakSelf imageSavedAtPath:path
                           framingCoordinates:coordinates
                                   completion:std::move(finalCompletion)];
                 },
                 std::move(completion)));
}

#pragma mark - Private

- (void)imageSavedAtPath:(base::FilePath)imagePath
      framingCoordinates:(HomeCustomizationFramingCoordinates*)coordinates
              completion:(base::OnceClosure)completion {
  if (!imagePath.empty() && _backgroundService) {
    _backgroundService->SetCurrentUserUploadedBackground(
        imagePath.value(),
        FramingCoordinatesFromHomeCustomizationFramingCoordinates(coordinates));
    _backgroundService->StoreCurrentTheme();
  }
  std::move(completion).Run();
}

@end
