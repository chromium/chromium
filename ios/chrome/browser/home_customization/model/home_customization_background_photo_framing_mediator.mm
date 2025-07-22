// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mediator.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/home_customization/model/framing_coordinates.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mutator.h"

@implementation HomeCustomizationBackgroundPhotoFramingMediator {
  // Task runner for file operations operations.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // File path for profile-specific storage.
  base::FilePath _imageSavePath;
  PhotoSelectionFinishedCommand _completionCommand;
  raw_ptr<HomeBackgroundCustomizationService> _backgroundService;
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

- (void)setCompletionCommand:(PhotoSelectionFinishedCommand)command {
  _completionCommand = [command copy];
}

- (void)saveImageWithFramingCoordinates:(UIImage*)image
                            coordinates:(HomeCustomizationFramingCoordinates*)
                                            coordinates {
  DCHECK(image);
  DCHECK(coordinates);
  DCHECK(_completionCommand);

  UIImage* imageCopy = [image copy];
  HomeCustomizationFramingCoordinates* coordinatesCopy = [coordinates copy];

  [self performBackgroundImageSaveWithCommand:imageCopy
                                  coordinates:coordinatesCopy
                            completionCommand:_completionCommand];
}

#pragma mark - Private

// Performs background save and calls command when complete.
- (void)performBackgroundImageSaveWithCommand:(UIImage*)image
                                  coordinates:
                                      (HomeCustomizationFramingCoordinates*)
                                          coordinates
                            completionCommand:
                                (PhotoSelectionFinishedCommand)command {
  __weak __typeof(self) weakSelf = self;
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          __strong __typeof(weakSelf) strongSelf = weakSelf;
                          if (!strongSelf) {
                            dispatch_async(dispatch_get_main_queue(), ^{
                              command();
                            });
                            return;
                          }

                          [strongSelf saveImageOnBackgroundThread:image
                                                      coordinates:coordinates
                                                completionCommand:command];
                        }));
}

// Performs the image save on background thread.
- (void)saveImageOnBackgroundThread:(UIImage*)image
                        coordinates:
                            (HomeCustomizationFramingCoordinates*)coordinates
                  completionCommand:(PhotoSelectionFinishedCommand)command {
  // Save image to profile directory.
  NSString* imagePath = [self saveImageToProfileDirectory:image];

  // Convert coordinates to C++ struct.
  FramingCoordinates cppCoordinates = [coordinates toFramingCoordinates];

  // Return to main thread with results.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (imagePath && self->_backgroundService) {
      self->_backgroundService->SetCurrentUserUploadedBackground(
          base::SysNSStringToUTF8(imagePath), cppCoordinates);
    }

    command();
  });
}

#pragma mark - Private

// Compress and saves the image to the profile directory.
- (NSString*)saveImageToProfileDirectory:(UIImage*)image {
  // Get profile-specific path.
  base::FilePath backgroundImagesPath =
      _imageSavePath.AppendASCII("BackgroundImages");

  // Create directory if it doesn't exist.
  if (!base::CreateDirectory(backgroundImagesPath)) {
    LOG(ERROR) << "Failed to create directory: "
               << backgroundImagesPath.value();
    return nil;
  }

  // Generate UUID-based filename.
  NSString* uuid = [[NSUUID UUID] UUIDString];
  NSString* filename =
      [NSString stringWithFormat:@"background_image_%@.jpg", uuid];
  base::FilePath filePath =
      backgroundImagesPath.AppendASCII(base::SysNSStringToUTF8(filename));

  // Convert image to JPEG and save.
  NSData* imageData = UIImageJPEGRepresentation(image, 0.9);
  if (!imageData) {
    return nil;
  }

  std::string dataString(reinterpret_cast<const char*>([imageData bytes]),
                         [imageData length]);
  if (!base::WriteFile(filePath, dataString)) {
    LOG(ERROR) << "Failed to write file: " << filePath.value();
    return nil;
  }

  return base::SysUTF8ToNSString(filePath.value());
}

@end
