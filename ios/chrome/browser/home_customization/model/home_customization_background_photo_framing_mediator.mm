// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mediator.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback_forward.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "ios/chrome/browser/home_customization/model/framing_coordinates.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mutator.h"

namespace {
// Compresses and saves `image` to the provided `directory_path`. Also generates
// a UUID-based filename for `image` and returns the full save path (or an empty
// path if saving failed).
base::FilePath SaveImageToDirectory(const base::FilePath& directory_path,
                                    UIImage* image) {
  // Create directory if it doesn't exist.
  if (!base::CreateDirectory(directory_path)) {
    LOG(ERROR) << "Failed to create directory: " << directory_path.value();
    return base::FilePath();
  }

  // Convert image to JPEG.
  NSData* image_data = UIImageJPEGRepresentation(image, 0.9);
  if (!image_data) {
    return base::FilePath();
  }

  // Generate UUID-based filename.
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  base::FilePath file_path = directory_path.AppendASCII(
      base::StrCat({"background_image_", uuid.AsLowercaseString(), ".jpg"}));

  const std::string_view data_string(
      reinterpret_cast<const char*>([image_data bytes]), [image_data length]);
  if (!base::WriteFile(file_path, data_string)) {
    LOG(ERROR) << "Failed to write file: " << file_path.value();
    return base::FilePath();
  }

  return file_path;
}
}  // namespace

@implementation HomeCustomizationBackgroundPhotoFramingMediator {
  // Task runner for file operations operations.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // File path for profile-specific storage.
  base::FilePath _imageSavePath;
  raw_ptr<HomeBackgroundCustomizationService> _backgroundService;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithFilePath:(const base::FilePath&)filePath
               backgroundService:
                   (HomeBackgroundCustomizationService*)backgroundService {
  self = [super init];
  if (self) {
    _imageSavePath = filePath;
    _backgroundService = backgroundService;
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  return self;
}

#pragma mark - HomeCustomizationBackgroundPhotoFramingMutator

- (void)saveImage:(UIImage*)image
    withFramingCoordinates:(HomeCustomizationFramingCoordinates*)coordinates
                completion:(base::OnceClosure)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(image);
  DCHECK(coordinates);

  // Get profile-specific path.
  base::FilePath backgroundImagesPath =
      _imageSavePath.AppendASCII("BackgroundImages");

  __weak __typeof(self) weakSelf = self;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveImageToDirectory, backgroundImagesPath, image),
      base::BindOnce(
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!imagePath.empty() && _backgroundService) {
    _backgroundService->SetCurrentUserUploadedBackground(
        imagePath.value(), [coordinates toFramingCoordinates]);
  }
  std::move(completion).Run();
}

@end
