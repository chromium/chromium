// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_DELEGATE_H_

// Delegate for the OpenInActivityViewController class.
@protocol OpenInActivityDelegate

// Method invoked when the OpenInActivityViewController is about to be removed.
- (void)openInActivityWillDisappearForFileAtURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_DELEGATE_H_
