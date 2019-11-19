// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_manager.h"

#import "ios/web/favicon/favicon_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kCommandPrefix[] = "favicon";
}

namespace web {

FaviconManager::FaviconManager(WebStateImpl* web_state)
    : web_state_impl_(web_state) {
  subscription_ = web_state_impl_->AddScriptCommandCallback(
      base::BindRepeating(&FaviconManager::OnJsMessage, base::Unretained(this)),
      kCommandPrefix);
}

FaviconManager::~FaviconManager() {}

void FaviconManager::OnJsMessage(const base::DictionaryValue& message,
                                 const GURL& page_url,
                                 bool has_user_gesture,
                                 WebFrame* sender_frame) {
  DCHECK(sender_frame->IsMainFrame());

  const std::string* command = message.FindStringKey("command");
  if (!command) {
    return;
  }

  std::vector<FaviconURL> URLs;
  if (!ExtractFaviconURL(&message, page_url, &URLs))
    return;

  if (!URLs.empty())
    web_state_impl_->OnFaviconUrlUpdated(URLs);
}

}  // namespace web
