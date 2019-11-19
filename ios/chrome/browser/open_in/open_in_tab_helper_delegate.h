// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_DELEGATE_H_

class GURL;

namespace web {
class WebState;
}

@class NSString;

// Protocol for handling openIn and presenting related UI.
@protocol OpenInTabHelperDelegate

// Enables the openIn view for the webState with the |documentURL| and sets
// the file name for the currently loaded document.
- (void)enableOpenInForWebState:(web::WebState*)webState
                withDocumentURL:(const GURL&)documentURL
              suggestedFileName:(NSString*)suggestedFileName;

// Disables the openIn view for |webState|.
- (void)disableOpenInForWebState:(web::WebState*)webState;

// Destroys the openIn view and detach it from the |webState|.
- (void)destroyOpenInForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_DELEGATE_H_
