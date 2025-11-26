// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_framing_mediator.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"

namespace {

// Records the file size to UMA histogram if it exists.
void RecordUserUploadedImageManagerFileSize(base::FilePath full_image_path) {
  base::ScopedBlockingCall may_block(FROM_HERE, base::BlockingType::MAY_BLOCK);

  std::optional<int64_t> file_size = base::GetFileSize(full_image_path);
  if (file_size.has_value()) {
    base::UmaHistogramMemoryKB(
        "IOS.HomeCustomization.Background.UserUploaded.ImageSizeInKB",
        file_size.value() / 1024);
  }
}

}  // namespace

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
                completion:(base::OnceCallback<void(BOOL success)>)completion {
  DCHECK(image);
  DCHECK(coordinates);

  __weak __typeof(self) weakSelf = self;
  _userUploadedImageManager->StoreUserUploadedImage(
      image, base::BindOnce(^BOOL(base::FilePath path) {
               return [weakSelf imageSavedAtPath:path
                              framingCoordinates:coordinates];
             }).Then(std::move(completion)));
}

#pragma mark - Private

// Completion handler for when the image is saved. Returns YES if the save is
// successful and NO otherwise.
- (BOOL)imageSavedAtPath:(base::FilePath)imagePath
      framingCoordinates:(HomeCustomizationFramingCoordinates*)coordinates {
  if (!imagePath.empty() && _backgroundService) {
    _backgroundService->SetCurrentUserUploadedBackground(
        imagePath.value(),
        FramingCoordinatesFromHomeCustomizationFramingCoordinates(coordinates));

    base::FilePath fullImagePath =
        _userUploadedImageManager->GetFullImagePath(imagePath);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&RecordUserUploadedImageManagerFileSize, fullImagePath));
  }

  return !imagePath.empty();
}

@end
