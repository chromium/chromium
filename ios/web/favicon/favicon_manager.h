// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_MANAGER_H_
#define IOS_WEB_FAVICON_FAVICON_MANAGER_H_

#include "base/macros.h"
#include "base/values.h"
#import "ios/web/web_state/web_state_impl.h"

class GURL;
namespace web {
class WebFrame;

// Handles "favicon.favicons" message from injected JavaScript and notifies
// WebStateImpl if message contains favicon URLs.
class FaviconManager final {
 public:
  explicit FaviconManager(WebStateImpl* web_state);
  ~FaviconManager();

 private:
  void OnJsMessage(const base::DictionaryValue& message,
                   const GURL& page_url,
                   bool has_user_gesture,
                   WebFrame* sender_frame);

  WebStateImpl* web_state_impl_ = nullptr;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(FaviconManager);
};

}  // namespace web

#endif  // IOS_WEB_FAVICON_FAVICON_MANAGER_H_
