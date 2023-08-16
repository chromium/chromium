// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_PHOTOS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_PHOTOS_COMMANDS_H_

@class OpenSaveToPhotosAccountPickerCommand;
@class ContinueSaveImageToPhotosCommand;
@class SaveImageToPhotosCommand;

// Commands related to Save to Photos.
@protocol SaveToPhotosCommands

// Starts Save to Photos by requesting to save an image.
- (void)saveImageToPhotos:(SaveImageToPhotosCommand*)command;

// Stops Save to Photos.
- (void)stopSaveToPhotos;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_PHOTOS_COMMANDS_H_
