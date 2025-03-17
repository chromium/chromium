// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_
#define IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"

@class WKWebsiteDataStore;

namespace base {
class Uuid;
}

namespace web {

class BrowserState;

// Returns a data store associated with BrowserState.
WKWebsiteDataStore* GetDataStoreForBrowserState(BrowserState* browser_state);

// Delete the storage associated with uuid. This must only be called if no
// storage is created for that identifier.
void RemoveDataStorageForIdentifier(
    const base::Uuid& uuid,
    base::OnceCallback<void(NSError*)> callback);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_
