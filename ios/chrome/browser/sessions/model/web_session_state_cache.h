// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_H_

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/functional/callback_forward.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace web {
class WebStateID;
}

// A profile keyed service, providing an on-disk cache of WKWebView
// sessionState data, modeled after the SnapshotStorage. Data is persisted to
// disk in a background thread with the provided NSData to a file name based on
// the webState TabId. Data can be written or deleted by tabId, delayed during
// batch operations, or purged based on any nonexistent tabIds.
@interface WebSessionStateCache : NSObject

// Designated initializer.
- (instancetype)initWithBrowserState:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Persists `data` in a background thread based on `webStateID`.
- (void)persistSessionStateData:(NSData*)data
                  forWebStateID:(web::WebStateID)webStateID;

// Retrieves the persisted session state based on `webStateID`.  Returns
// nil if file does not exist.
- (NSData*)sessionStateDataForWebStateID:(web::WebStateID)webStateID;

// Deletes the persisted session state based on `webStateID` in a background
// thread.  If `_delayRemove` is set, purge is instead called on a short delay.
- (void)removeSessionStateDataForWebStateID:(web::WebStateID)webStateID
                                  incognito:(BOOL)incognito;

// Removes any persisted session data for tabs that no longer exist and
// invokes `closure` on the calling sequence when the operation completes.
- (void)purgeUnassociatedDataWithCompletion:(base::OnceClosure)closure;

// Delay any removes triggered by -removeSessionStateDataForWebState.  This is
// useful when doing a 'Close All' -> 'Undo' in the tab grid.
- (void)setDelayRemove:(BOOL)delayRemove;

// Invoked before the instance is deallocated. Needs to release all reference
// to C++ objects. Object will soon be deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_H_
