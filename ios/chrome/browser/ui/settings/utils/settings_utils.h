// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_SETTINGS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_SETTINGS_UTILS_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class GURL;

typedef void (^ProceduralBlockWithURL)(const GURL&);

// Returns a ProceduralBlockWithURL that uses the dispatcher and opens url
// (parameter to the block) in a new tab.
ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder,
                                      id<ApplicationCommands> handler);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_SETTINGS_UTILS_H_
