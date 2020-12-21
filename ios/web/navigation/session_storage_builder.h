// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_
#define IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_

@class CRWSessionStorage;

namespace web {

class WebStateImpl;

// Allow navigation items up to ~63k (like components/sessions/core)
const int kMaxNavigationItemSize = 63 * 1024;

// Class that can serialize and deserialize session information.
class SessionStorageBuilder {
 public:
  // Creates a serializable session storage from |web_state|.
  CRWSessionStorage* BuildStorage(WebStateImpl* web_state) const;
  // Populates |web_state| with |storage|'s session information.
  // The provided |web_state| must already have a |NavigationManager|.
  void ExtractSessionState(WebStateImpl* web_state,
                           CRWSessionStorage* storage) const;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_
