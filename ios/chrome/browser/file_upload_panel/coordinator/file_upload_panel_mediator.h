// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_

#import <Foundation/Foundation.h>

class ChooseFileController;
@protocol FileUploadPanelCommands;

// Mediator for the file upload panel UI.
API_AVAILABLE(ios(18.4))
@interface FileUploadPanelMediator : NSObject

// Handler for the file upload panel UI.
@property(nonatomic, weak) id<FileUploadPanelCommands> fileUploadPanelHandler;

// Initializes the file upload panel and binds it to `controller`.
- (instancetype)initWithChooseFileController:(ChooseFileController*)controller
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the file upload panel from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_MEDIATOR_H_
