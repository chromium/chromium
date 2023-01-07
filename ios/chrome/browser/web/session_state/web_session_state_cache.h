// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_H_
#define IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_H_

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
namespace web {
class WebState;
}

// The name of the subdirectory used to store the on-disk cache of sessionState
// data.
extern const base::FilePath::CharType kWebSessionCacheDirectoryName[];

// A browser state keyed service, providing an on-disk cache of WKWebView
// sessionState data, modeled after the SnapshotCache.  Data is persisted to
// disk in a background thread with the provided NSData to a file name based on
// the webState TabId. Data can be written or deleted by tabId, delayed during
// batch operations, or purged based on any nonexistent tabIds.
@interface WebSessionStateCache : NSObject

// Designated initializer.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Persists `data` in a background thread based on `webState`'s tab id.
- (void)persistSessionStateData:(NSData*)data
                    forWebState:(const web::WebState*)webState;

// Retrieves the persisted session state based on `webState`'s tab id.  Returns
// nil if file does not exist.
- (NSData*)sessionStateDataForWebState:(const web::WebState*)webState;

// Deletes the persisted session state based on `webState`'s tab id in a
// background thread.  If `_delayRemove` is set, purge is instead called on a
// short delay.
- (void)removeSessionStateDataForWebState:(const web::WebState*)webState;

// Removes any persisted session data for tabs that no longer exist.  Usually
// this happens because of a crash, but may also be used internally if any tabs
// are removed when `_delayRemove` is true.
- (void)purgeUnassociatedData;

// Delay any removes triggered by -removeSessionStateDataForWebState.  This is
// useful when doing a 'Close All' -> 'Undo' in the tab grid.
- (void)setDelayRemove:(BOOL)delayRemove;

// Invoked before the instance is deallocated. Needs to release all reference
// to C++ objects. Object will soon be deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_H_
