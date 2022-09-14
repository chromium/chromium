// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_FAVICON_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_FAVICON_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

// Protocol that the tabstrip UI uses to asynchronously pull favicons for cells
// in the tabstrip.
@protocol TabFaviconDataSource

// Requests the receiver to provide a favicon image corresponding to
// `identifier`. `completion` is called with the image if it exists.
- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_FAVICON_DATA_SOURCE_H_
