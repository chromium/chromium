// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SESSION_STORAGE_BUILDER_H_
#define IOS_WEB_NAVIGATION_SESSION_STORAGE_BUILDER_H_

@class CRWSessionStorage;

namespace web {

class NavigationManagerImpl;
class SessionCertificatePolicyCacheImpl;
class WebStateImpl;

// Class that can serialize and deserialize session information.
class SessionStorageBuilder {
 public:
  // Creates a serializable session storage from `web_state`,
  // `navigation_manager` and `session_certificate_policy_cache`.
  static CRWSessionStorage* BuildStorage(
      const WebStateImpl& web_state,
      const NavigationManagerImpl& navigation_manager,
      const SessionCertificatePolicyCacheImpl&
          session_certificate_policy_cache);

  // Populates `web_state` and it's `navigation_manager` with `storage`'s
  // session information.
  static void ExtractSessionState(WebStateImpl& web_state,
                                  NavigationManagerImpl& navigation_manager,
                                  CRWSessionStorage* storage);

  SessionStorageBuilder() = delete;
  ~SessionStorageBuilder() = delete;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SESSION_STORAGE_BUILDER_H_
