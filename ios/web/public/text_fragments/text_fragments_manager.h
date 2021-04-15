// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_
#define IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_

#import "ios/web/public/web_state_user_data.h"

namespace web {

// Handles highlighting of text fragments on the page and user interactions
// with these highlights.
class TextFragmentsManager : public WebStateUserData<TextFragmentsManager> {
 public:
  ~TextFragmentsManager() override {}
  TextFragmentsManager(const TextFragmentsManager&) = delete;
  TextFragmentsManager& operator=(const TextFragmentsManager&) = delete;

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  TextFragmentsManager() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_
