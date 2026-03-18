// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_COORDINATOR_FULLSCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_COORDINATOR_FULLSCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

// The mediator for the fullscreen feature.
@interface FullscreenMediator : NSObject

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_COORDINATOR_FULLSCREEN_MEDIATOR_H_
