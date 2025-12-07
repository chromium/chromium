// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_LIBRARY_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_LIBRARY_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller responsible for presenting images from the user's camera roll
// in the Home customization flow. Uses a collection view to display selectable
// background images.
@interface HomeCustomizationBackgroundPhotoLibraryPickerViewController
    : UIViewController <UICollectionViewDelegate, UICollectionViewDataSource>

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_LIBRARY_PICKER_VIEW_CONTROLLER_H_
