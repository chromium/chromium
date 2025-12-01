// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"

class ChooseFileController;
struct ChooseFileEvent;
@protocol FileUploadPanelCommands;

// Mediator for the file upload panel UI.
API_AVAILABLE(ios(18.4))
@interface FileUploadPanelMediator : NSObject

// Handler for the file upload panel UI.
@property(nonatomic, weak) id<FileUploadPanelCommands> fileUploadPanelHandler;

// ChooseFileEvent object associated with this presentation of the upload panel.
@property(nonatomic, readonly) const ChooseFileEvent& event;
// The location in the screen where the file input was triggered.
@property(nonatomic, readonly) CGPoint eventScreenLocation;
// Capture type associated with the file upload element.
@property(nonatomic, readonly) ChooseFileCaptureType eventCaptureType;
// Media types accepted by the file upload element.
@property(nonatomic, readonly) NSSet<NSString*>* acceptedMediaTypes;
// Document types accepted by the file upload element i.e. only folders if
// `allowsDirectorySelection` is YES, or `acceptedMediaTypes` if it is not
// empty, or all files types otherwise.
@property(nonatomic, readonly) NSArray<UTType*>* acceptedDocumentTypes;
// Media types available for the camera and accepted by the file upload element.
@property(nonatomic, readonly)
    NSArray<NSString*>* acceptedMediaTypesAvailableForCamera;
// Preferred camera device as a function of `eventCaptureType`.
@property(nonatomic, readonly)
    UIImagePickerControllerCameraDevice preferredCameraDevice;
// Whether the camera should be shown directly.
@property(nonatomic, readonly) BOOL shouldShowCamera;
// Whether the page allows the selection of images.
@property(nonatomic, readonly) BOOL allowsImageSelection;
// Whether the page allows the selection of videos.
@property(nonatomic, readonly) BOOL allowsVideoSelection;
// Whether the page allows the selection of media items (images or video).
@property(nonatomic, readonly) BOOL allowsMediaSelection;
// Whether the page allows the selection a directory.
@property(nonatomic, readonly) BOOL allowsDirectorySelection;
// Whether the page allows multiple selection.
@property(nonatomic, readonly) BOOL allowsMultipleSelection;

// Initializes the file upload panel and binds it to `controller`.
- (instancetype)initWithChooseFileController:(ChooseFileController*)controller
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Adjusts the capture type according to available devices.
- (void)adjustCaptureTypeToAvailableDevices;
// Submit a file selection according to media info from the camera.
- (void)submitFileSelectionWithMediaInfo:
    (NSDictionary<UIImagePickerControllerInfoKey, id>*)info;
// Submit a file selection according to picker results.
- (void)submitFileSelectionWithPickerResults:(NSArray<PHPickerResult*>*)results;
// Submit a list of file URLs as selection.
- (void)submitFileSelection:(NSArray<NSURL*>*)fileURLs;
// Cancels file selection.
- (void)cancelFileSelection;

// Disconnects the file upload panel from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_
