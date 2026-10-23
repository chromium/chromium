// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_

#import <Foundation/Foundation.h>

#import <memory>

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

enum class AssistantAimEntrypoint;
@class ComposeboxInputItem;
class GURL;

// Context for the Cobrowse flow.
@interface CobrowseContext : NSObject

// The triggering URL.
@property(nonatomic, assign, readonly) const GURL& url;

// The search query text.
// Corresponds to the "q" query parameter.
@property(nonatomic, copy, readonly) NSString* searchQuery;

// The server-side ID of the conversation.
// Corresponds to the "mtid" query parameter.
@property(nonatomic, copy, readonly) NSString* serverID;

// The turn ID (state token) of the conversation.
// Corresponds to the "mstk" query parameter.
@property(nonatomic, copy, readonly) NSString* turnID;

// The tab items attached from the composebox.
@property(nonatomic, copy) NSArray<ComposeboxInputItem*>* attachedTabItems;

// Whether this context holds a transferable contextual search session handle.
@property(nonatomic, assign, readonly) BOOL hasContextualSearchSession;

// Entrypoint that initiated the cobrowse session.
@property(nonatomic, assign) AssistantAimEntrypoint entrypoint;

// Whether the context contains server session tokens (mstk, mtid, or cinpts).
@property(nonatomic, assign, readonly) BOOL hasServerSessionTokens;

// Sets the contextual search session handle transferred from the composebox.
- (void)setContextualSearchSession:
    (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)session;

// Transfers ownership of the stored contextual search session handle to the
// caller, leaving the context's session handle empty.
- (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
    takeContextualSearchSession;

// Transfers and clears the attached tab items stored in this context.
- (NSArray<ComposeboxInputItem*>*)takeAttachedTabItems;

// Returns a default context with the base search URL, used mainly for debugging
// purposes.
+ (instancetype)defaultContext;

// Returns a context initialized with `searchQuery`, using the default base
// search URL.
+ (instancetype)cobrowseContextWithSearchQuery:(NSString*)searchQuery;

// Initializes the context with `url`, adding cobrowse query parameters.
// Note: `url` may be ignored if the "CobrowseGwsURL" experimental setting is
// set.
- (instancetype)initWithURL:(const GURL&)url NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_CONTEXT_H_
