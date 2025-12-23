// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_NAVBAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_NAVBAR_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@interface AssistantNavbarConfiguration : NSObject

@property(nonatomic, copy) NSString* title;

// TODO(crbug.com/469050167): Add support for leading and trailing buttons.
// @property(nonatomic, copy) NSArray<AssistantNavbarButton*>* leadingButtons;
// @property(nonatomic, copy) NSArray<AssistantNavbarButton*>* trailingButtons;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_NAVBAR_CONFIGURATION_H_
