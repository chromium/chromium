// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_NAVIGATION_API_H_
#define IOS_PUBLIC_PROVIDER_WEB_NAVIGATION_API_H_

@class WKBackForwardListItem;

namespace web::provider {

// Returns whether `item` was created from an automatic navigation (i.e.
// without user interaction).
bool WasCreatedAutomatically(WKBackForwardListItem* item);

}  // namespace web::provider

#endif  // IOS_PUBLIC_PROVIDER_WEB_NAVIGATION_API_H_
