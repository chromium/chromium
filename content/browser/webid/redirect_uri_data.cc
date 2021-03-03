// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/redirect_uri_data.h"

#include "content/public/browser/web_contents.h"

namespace content {

constexpr char kRedirectUriKey[] = "kRedirectUriKey";

RedirectUriData::RedirectUriData(std::string redirect_uri)
    : redirect_uri_(std::move(redirect_uri)) {}
RedirectUriData::~RedirectUriData() = default;

// static
void RedirectUriData::Set(WebContents* web_contents, std::string redirect_uri) {
  web_contents->SetUserData(kRedirectUriKey, std::make_unique<RedirectUriData>(
                                                 std::move(redirect_uri)));
}

// static
RedirectUriData* RedirectUriData::Get(WebContents* web_contents) {
  return static_cast<RedirectUriData*>(
      web_contents->GetUserData(kRedirectUriKey));
}

// static
void RedirectUriData::Remove(WebContents* web_contents) {
  web_contents->RemoveUserData(kRedirectUriKey);
}

std::string RedirectUriData::Value() {
  return redirect_uri_;
}

}  // namespace content
