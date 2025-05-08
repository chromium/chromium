// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_

#import <Foundation/Foundation.h>

// Mediator for the reader mode options.
@interface ReaderModeOptionsMediator : NSObject

// Disconnects from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_
