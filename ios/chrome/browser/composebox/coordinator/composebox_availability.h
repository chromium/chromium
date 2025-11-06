// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_AVAILABILITY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"

class Browser;

/// Maybe shows the composebox and returns whether it has been shown.
bool MaybeShowComposebox(Browser* browser,
                         ComposeboxEntrypoint entrypoint,
                         NSString* query = nil);

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_AVAILABILITY_H_
