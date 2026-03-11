// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_

#import <Foundation/Foundation.h>

class GURL;

// Context for the Cobrowse flow.
@interface CobrowseContext : NSObject

// The triggering URL.
@property(nonatomic, assign, readonly) const GURL& url;

// The unique server-side identifier for this specific conversation turn.
// Corresponds to the "mstk" query parameter.
@property(nonatomic, copy, readonly) NSString* conversationTurnID;

// The search query text.
// Corresponds to the "q" query parameter.
@property(nonatomic, copy, readonly) NSString* searchQuery;

// The server-side ID of the conversation.
// Corresponds to the "mtid" query parameter.
@property(nonatomic, copy, readonly) NSString* serverID;

// Initializes the context with `url`, adding cobrowse query parameters.
- (instancetype)initWithURL:(const GURL&)url NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_
