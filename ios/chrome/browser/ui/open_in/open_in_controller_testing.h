// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_TESTING_H_

@interface OpenInController (TestingAditions)
- (NSString*)suggestedFilename;
- (void)startDownload;
@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_TESTING_H_
