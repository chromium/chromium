// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Presentation delegate for SaveToPhotosSettingsAccountSelectionViewController.
// Invoked when the view controller is dismissed.
@protocol
    SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate <
        NSObject>

// Invoked when the SaveToPhotosSettingsAccountSelectionViewController is
// dismissed.
- (void)saveToPhotosSettingsAccountSelectionViewControllerWasRemoved;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
