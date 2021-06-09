// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_HIGHLIGHTED_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_HIGHLIGHTED_BUTTON_H_

#import <UIKit/UIKit.h>

// A UIButton subclass that applies a lower alpha when the button is
// highlighted. When the button isn't highlighted, the alpha is set to 1.
@interface HighlightedButton : UIButton

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_HIGHLIGHTED_BUTTON_H_
