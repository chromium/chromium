// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_

#import <UIKit/UIKit.h>

// A detent with an identifier and a dynamic value.
@interface AssistantContainerDetent : NSObject

// The unique identifier for this detent.
@property(nonatomic, readonly, copy) NSString* identifier;

// The resolved value for the detent.
@property(nonatomic, readonly) NSInteger value;

// Creates a new detent with a given identifier and a block providing an integer
// value for the detent.
- (instancetype)initWithIdentifier:(NSString*)identifier
                     valueResolver:(NSInteger (^)())valueResolver;

// Unavailable.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_H_
