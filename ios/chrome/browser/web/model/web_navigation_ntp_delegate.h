// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_NTP_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_NTP_DELEGATE_H_

#include <CoreFoundation/CoreFoundation.h>

namespace web {
class WebState;
}

// Delegate protocol for web navigation interactions with the New Tab Page.
// This is a temporary protocol, used as long as parts of the NTP implementation
// are owned by the UI layer (Specifically: the BVC).
@protocol WebNavigationNTPDelegate

// YES if the delegate determines that the current web state is showing an
// NTP.
// (Note that it is implied that the delegate and the delegating object share
// a common understanding of what the current web state is; that is, they are
// both scoped to the same WebStateList (or Browser)).
@property(nonatomic, readonly, getter=isNTPActiveForCurrentWebState)
    BOOL NTPActiveForCurrentWebState;

// Tells the delegate to reload the NTP for `webState`, if any.
- (void)reloadNTPForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_NTP_DELEGATE_H_
