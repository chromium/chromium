// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_H_

@protocol OmniboxPedal <NSObject>

@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) NSString* subtitle;
@property(nonatomic, readonly) void (^action)(void);
/// This is actually an `Int`-casted `OmniboxPedalId`, but that C++ `enum class`
/// can't be used due to swift interop.
@property(nonatomic, readonly, assign) NSInteger type;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_H_
