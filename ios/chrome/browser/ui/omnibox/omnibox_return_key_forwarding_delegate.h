// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_RETURN_KEY_FORWARDING_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_RETURN_KEY_FORWARDING_DELEGATE_H_

#import "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"

// A ObjC-to-C++ bridge between <OmniboxReturnDelegate> and
// OmniboxTextAcceptDelegate.
@interface ForwardingReturnDelegate : NSObject <OmniboxReturnDelegate>

- (void)setAcceptDelegate:(OmniboxTextAcceptDelegate*)delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_RETURN_KEY_FORWARDING_DELEGATE_H_
