// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/memory/ref_counted.h"
#import "ios/chrome/browser/ui/open_in/open_in_toolbar.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace web {
class WebState;
}

// Enum for the IOS.OpenIn.DownloadResult UMA histogram to log the result of
// the file download initiated when the user tap on "open in" button.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OpenInDownloadResult {
  kSucceeded = 0,
  kCanceled = 1,
  kFailed = 2,
  kMaxValue = kFailed,
};

// Class used to handle opening files in other applications.
@interface OpenInController : NSObject <UIGestureRecognizerDelegate,
                                        UIDocumentInteractionControllerDelegate>
// Designated initializer.
- (id)initWithURLLoaderFactory:
          (scoped_refptr<network::SharedURLLoaderFactory>)urlLoaderFactory
                      webState:(web::WebState*)webState;

// Base view on which the Open In toolbar will be presented.
@property(nonatomic, weak) UIView* baseView;

// Removes the |openInToolbar_| from the |webController_|'s view and resets the
// variables specific to the loaded document.
- (void)disable;

// Disconnects the controller from its WebState. Should be called when the
// WebState is being torn down.
- (void)detachFromWebState;

// Adds the |openInToolbar_| to the |webController_|'s view and sets the url and
// the filename for the currently loaded document.
- (void)enableWithDocumentURL:(const GURL&)documentURL
            suggestedFilename:(NSString*)suggestedFilename;
@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_CONTROLLER_H_
