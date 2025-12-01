// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_media_item.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_picker_result_loader.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_observer_bridge.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Compression level for images captured with the camera.
constexpr CGFloat kCompressionForImageJPEGRepresentation = 0.8;
// File name for camera images written to a temporary location on disk.
constexpr char kCameraImageFileName[] = "image.jpg";

// Returns the type identifiers associated with `mime_types`. Returns nil to
// indicate that `mime_types` matches all UTIs.
NSSet<NSString*>* MediaTypeIdentifiersForMIMETypes(
    const std::vector<std::string>& mime_types) {
  NSMutableSet* type_identifiers = [NSMutableSet set];
  for (const std::string& mime_type : mime_types) {
    const std::string lowercase_mime_type = base::ToLowerASCII(mime_type);
    if (lowercase_mime_type == "*/*") {
      return nil;
    }
    if (lowercase_mime_type == "image/*") {
      [type_identifiers addObject:UTTypeImage.identifier];
    } else if (lowercase_mime_type == "video/*" ||
               lowercase_mime_type == "audio/*") {
      // Adding UTTypeMovie for "audio/*" to allow recording with camera.
      [type_identifiers addObject:UTTypeMovie.identifier];
    } else {
      UTType* type = [UTType
          typeWithMIMEType:base::SysUTF8ToNSString(lowercase_mime_type)];
      if (type) {
        [type_identifiers addObject:type.identifier];
      }
    }
  }
  return type_identifiers;
}

// Returns all media types available for the camera for which at least one
// element in `accepted_types` conforms to that type. If `accepted_types` is
// empty then returns all media types available for the camera.
NSArray<NSString*>* CameraTypesAcceptedBySet(NSSet<NSString*>* accepted_types) {
  NSArray<NSString*>* all_camera_type_identifiers = [UIImagePickerController
      availableMediaTypesForSourceType:UIImagePickerControllerSourceTypeCamera];
  if (accepted_types.count) {
    NSMutableArray<NSString*>* accepted_camera_types = [NSMutableArray array];
    for (NSString* camera_type_identifier in all_camera_type_identifiers) {
      if ([accepted_types containsObject:camera_type_identifier]) {
        // If there is an element in `accepted_types` equal to
        // `camera_type_identifier` then it conforms to
        // `camera_type_identifier`.
        [accepted_camera_types addObject:camera_type_identifier];
      } else {
        // If no element in `accepted_types` is equal to
        // `camera_type_identifier` then maybe one of them still indirectly
        // conforms to it.
        UTType* camera_type =
            [UTType typeWithIdentifier:camera_type_identifier];
        if (FindTypeConformingToTarget(accepted_types, camera_type)) {
          [accepted_camera_types addObject:camera_type_identifier];
        }
      }
    }
    if (accepted_camera_types.count) {
      return accepted_camera_types;
    }
  }
  return all_camera_type_identifiers;
}

// Writes the JPEG representation of `image` to a temporary location associated
// with the tab identified by `web_state_id`. If successful, returns the
// destination path of the image.
std::optional<base::FilePath> WriteImageToTemporaryLocationForTab(
    UIImage* image,
    web::WebStateID web_state_id) {
  NSData* jpeg_representation =
      UIImageJPEGRepresentation(image, kCompressionForImageJPEGRepresentation);
  if (!jpeg_representation) {
    return std::nullopt;
  }
  std::optional<base::FilePath> directory =
      CreateTabChooseFileSubdirectory(web_state_id);
  if (!directory) {
    return std::nullopt;
  }
  const base::FilePath image_file_path =
      directory->Append(kCameraImageFileName);
  if (base::WriteFile(image_file_path,
                      base::apple::NSDataToSpan(jpeg_representation))) {
    return image_file_path;
  }
  return std::nullopt;
}

}  // namespace

@interface FileUploadPanelMediator () <ChooseFileControllerObserving>
@end

@implementation FileUploadPanelMediator {
  raw_ptr<ChooseFileController> _chooseFileController;
  std::unique_ptr<ChooseFileControllerObserverBridge>
      _chooseFileControllerObserverBridge;
  std::unique_ptr<base::ScopedObservation<ChooseFileController,
                                          ChooseFileController::Observer>>
      _chooseFileControllerObservation;
  ChooseFileCaptureType _eventCaptureType;
  NSSet<NSString*>* _acceptedMediaTypes;
  NSArray<NSString*>* _acceptedMediaTypesAvailableForCamera;
  NSArray<UTType*>* _acceptedDocumentTypes;
  web::WebStateID _webStateID;
  std::unique_ptr<FileUploadPanelPickerResultLoader> _pickerResultLoader;
}

#pragma mark - Initialization

- (instancetype)initWithChooseFileController:(ChooseFileController*)controller {
  self = [super init];
  if (self) {
    CHECK(controller);
    _chooseFileController = controller;
    _chooseFileControllerObserverBridge =
        std::make_unique<ChooseFileControllerObserverBridge>(self);
    _chooseFileControllerObservation = std::make_unique<base::ScopedObservation<
        ChooseFileController, ChooseFileController::Observer>>(
        _chooseFileControllerObserverBridge.get());
    _chooseFileControllerObservation->Observe(controller);
    _eventCaptureType = _chooseFileController->GetChooseFileEvent().capture;
    _webStateID = _chooseFileController->GetChooseFileEvent()
                      .web_state->GetUniqueIdentifier();
  }
  return self;
}

#pragma mark - Public properties

- (const ChooseFileEvent&)event {
  CHECK(_chooseFileController);
  return _chooseFileController->GetChooseFileEvent();
}

- (CGPoint)eventScreenLocation {
  return self.event.screen_location;
}

- (ChooseFileCaptureType)eventCaptureType {
  return _eventCaptureType;
}

- (NSSet<NSString*>*)acceptedMediaTypes {
  if (!_acceptedMediaTypes) {
    auto acceptedMimeTypes = self.event.accept_mime_types;
    for (const std::string& acceptedFileExtension :
         self.event.accept_file_extensions) {
      UTType* acceptedFileExtensionType = [UTType
          typeWithFilenameExtension:base::SysUTF8ToNSString(
                                        acceptedFileExtension.substr(1))];
      acceptedMimeTypes.push_back(
          base::SysNSStringToUTF8(acceptedFileExtensionType.preferredMIMEType));
    }
    _acceptedMediaTypes = MediaTypeIdentifiersForMIMETypes(acceptedMimeTypes);
  }
  return _acceptedMediaTypes;
}

- (NSArray<UTType*>*)acceptedDocumentTypes {
  if (!_acceptedDocumentTypes) {
    if (self.allowsDirectorySelection) {
      // If the input allows directory selection, then folders should be the
      // only accepted document type.
      _acceptedDocumentTypes = @[ UTTypeFolder ];
    } else if (self.acceptedMediaTypes.count == 0) {
      // If the list of accepted media types is empty, then any document type
      // can be selected.
      _acceptedDocumentTypes = @[ UTTypeItem ];
    } else {
      // If directories cannot be selected and the list of accepted media types
      // is not empty, then the accepted document types are the accepted media
      // types.
      NSMutableArray<UTType*>* acceptedDocumentTypes = [NSMutableArray array];
      for (NSString* acceptedMediaTypeIdentifier in self.acceptedMediaTypes) {
        UTType* acceptedMediaType =
            [UTType typeWithIdentifier:acceptedMediaTypeIdentifier];
        if (acceptedMediaType) {
          [acceptedDocumentTypes addObject:acceptedMediaType];
        }
      }
      _acceptedDocumentTypes = acceptedDocumentTypes;
    }
  }
  return _acceptedDocumentTypes;
}

- (NSArray<NSString*>*)acceptedMediaTypesAvailableForCamera {
  if (!_acceptedMediaTypesAvailableForCamera) {
    _acceptedMediaTypesAvailableForCamera =
        CameraTypesAcceptedBySet(self.acceptedMediaTypes);
  }
  return _acceptedMediaTypesAvailableForCamera;
}

- (UIImagePickerControllerCameraDevice)preferredCameraDevice {
  return self.eventCaptureType == ChooseFileCaptureType::kUser
             ? UIImagePickerControllerCameraDeviceFront
             : UIImagePickerControllerCameraDeviceRear;
}

- (BOOL)shouldShowCamera {
  if (_eventCaptureType == ChooseFileCaptureType::kNone ||
      ![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    return NO;
  }
  return YES;
}

- (BOOL)allowsImageSelection {
  return self.acceptedMediaTypes.count == 0 ||
         FindTypeConformingToTarget(self.acceptedMediaTypes, UTTypeImage);
}

- (BOOL)allowsVideoSelection {
  return self.acceptedMediaTypes.count == 0 ||
         FindTypeConformingToTarget(self.acceptedMediaTypes, UTTypeMovie);
}

- (BOOL)allowsMediaSelection {
  return self.allowsImageSelection || self.allowsVideoSelection;
}

- (BOOL)allowsDirectorySelection {
  return self.event.only_allow_directory;
}

- (BOOL)allowsMultipleSelection {
  return self.event.allow_multiple_files;
}

#pragma mark - Public

- (void)adjustCaptureTypeToAvailableDevices {
  if ([UIImagePickerController
          isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceFront] ||
      [UIImagePickerController
          isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear]) {
    if (![UIImagePickerController
            isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceFront]) {
      _eventCaptureType = ChooseFileCaptureType::kEnvironment;
    }
    if (![UIImagePickerController
            isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear]) {
      _eventCaptureType = ChooseFileCaptureType::kUser;
    }
    return;
  }
  _eventCaptureType = ChooseFileCaptureType::kNone;
}

- (void)submitFileSelectionWithMediaInfo:
    (NSDictionary<UIImagePickerControllerInfoKey, id>*)mediaInfo {
  NSString* mediaUTI = mediaInfo[UIImagePickerControllerMediaType];
  UTType* mediaType = [UTType typeWithIdentifier:mediaUTI];

  if ([mediaType conformsToType:UTTypeMovie]) {
    [self submitFileSelectionWithMediaURL:
              mediaInfo[UIImagePickerControllerMediaURL]];
    return;
  }

  CHECK([mediaType conformsToType:UTTypeImage])
      << "FileUploadPanelMediator: Media object should be a video or image.";
  CHECK_EQ(nil, mediaInfo[UIImagePickerControllerImageURL])
      << "FileUploadPanelMediator: Image URL should not be set.";
  UIImage* image = mediaInfo[UIImagePickerControllerOriginalImage];
  CHECK_NE(nil, image)
      << "FileUploadPanelMediator: Image should have image data.";

  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(WriteImageToTemporaryLocationForTab, image, _webStateID),
      base::BindOnce(^(std::optional<base::FilePath> imageFilePath) {
        [weakSelf submitFileSelectionWithImageFilePath:imageFilePath];
      }));
}

- (void)submitFileSelectionWithPickerResults:
    (NSArray<PHPickerResult*>*)results {
  [self loadAndTranscodeAndSubmitPickerResults:results];
}

- (void)submitFileSelection:(NSArray<NSURL*>*)fileURLs {
  if (_chooseFileController) {
    base::UmaHistogramCounts100("IOS.FileUploadPanel.SubmittedFileCount",
                                fileURLs.count);
    _chooseFileController->SubmitSelection(fileURLs, nil, nil);
  }
}

- (void)cancelFileSelection {
  [self submitFileSelection:nil];
}

- (void)disconnect {
  // If the controller still exists when the UI is being disconnect, cancel the
  // selection.
  [self cancelFileSelection];
  _pickerResultLoader.reset();
}

#pragma mark - ChooseFileControllerObserving

- (void)chooseFileControllerDestroyed:(ChooseFileController*)controller {
  _chooseFileController = nullptr;
  _chooseFileControllerObservation.reset();
  _chooseFileControllerObserverBridge.reset();
  [self.fileUploadPanelHandler hideFileUploadPanel];
}

#pragma mark - Private

// Submits file selection from `imageFilePath`.
- (void)submitFileSelectionWithImageFilePath:
    (std::optional<base::FilePath>)imageFilePath {
  NSURL* mediaURL = nil;
  if (imageFilePath) {
    mediaURL = base::apple::FilePathToNSURL(*imageFilePath);
  }
  [self submitFileSelectionWithMediaURL:mediaURL];
}

// Submits file selection from a `mediaURL`.
- (void)submitFileSelectionWithMediaURL:(NSURL*)mediaURL {
  if (mediaURL) {
    [self submitFileSelection:@[ mediaURL ]];
  } else {
    [self cancelFileSelection];
  }
}

// Asynchronously loads, transcodes and submits picker results.
- (void)loadAndTranscodeAndSubmitPickerResults:
    (NSArray<PHPickerResult*>*)results {
  __weak __typeof(self) weakSelf = self;
  _pickerResultLoader =
      std::make_unique<FileUploadPanelPickerResultLoader>(results, _webStateID);
  _pickerResultLoader->Load(
      base::BindOnce(^(NSArray<FileUploadPanelMediaItem*>* loadedItems) {
        [weakSelf handlePickerResultLoaderOutput:loadedItems];
      }));
}

// Submits the file selection for a list of transcoded items, if any.
// Cancels file selection if `loadedItems` is nil.
- (void)handlePickerResultLoaderOutput:
    (NSArray<FileUploadPanelMediaItem*>*)loadedItems {
  const auto loader = std::move(_pickerResultLoader);
  if (!loadedItems) {
    [self cancelFileSelection];
    return;
  }
  // TODO(crbug.com/441659098): Transcode and submit media items.
}

@end
