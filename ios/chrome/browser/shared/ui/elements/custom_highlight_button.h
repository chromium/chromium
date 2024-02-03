// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CUSTOM_HIGHLIGHT_BUTTON_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CUSTOM_HIGHLIGHT_BUTTON_H_

#import <UIKit/UIKit.h>

typedef void (^CustomHighlightableButtonHighlightHandler)(BOOL);

// A UIButton subclass for supporting custom highlight appearance.
// This class is created for less plumbing in use cases where the owner who can
// change the appearance is different from the accessor who wants to set the
// highlighted state, and there is no direct way for the accessor to call to the
// owner. With this class, the button is configured with the highlighting logic,
// and the accessor can directly invoke that through the button's API.
@interface CustomHighlightableButton : UIButton

// Sets the handler, which will be retained by the button and invoked when
// -setCustomHighlighted: is called.
- (void)setCustomHighlightHandler:
    (CustomHighlightableButtonHighlightHandler)customHighlightHandler;

// Sets the customHighlighted state of the button, which will invoke the handler
// passed in -customHighlightHandler. no-op if no handler has been passed.
- (void)setCustomHighlighted:(BOOL)customHighlighted;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CUSTOM_HIGHLIGHT_BUTTON_H_
