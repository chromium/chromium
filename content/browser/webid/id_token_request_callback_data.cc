// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/id_token_request_callback_data.h"

#include "content/public/browser/web_contents.h"

namespace content {

constexpr char kIdTokenRequestCallbackKey[] = "kIdTokenRequestCallbackKey";

IdTokenRequestCallbackData::IdTokenRequestCallbackData(DoneCallback callback)
    : callback_(std::move(callback)) {}
IdTokenRequestCallbackData::~IdTokenRequestCallbackData() = default;

DoneCallback IdTokenRequestCallbackData::TakeDoneCallback() {
  return std::move(callback_);
}

// static
void IdTokenRequestCallbackData::Set(WebContents* web_contents,
                                     DoneCallback callback) {
  web_contents->SetUserData(
      kIdTokenRequestCallbackKey,
      std::make_unique<IdTokenRequestCallbackData>(std::move(callback)));
}

// static
IdTokenRequestCallbackData* IdTokenRequestCallbackData::Get(
    WebContents* web_contents) {
  return static_cast<IdTokenRequestCallbackData*>(
      web_contents->GetUserData(kIdTokenRequestCallbackKey));
}

// static
void IdTokenRequestCallbackData::Remove(WebContents* web_contents) {
  web_contents->RemoveUserData(kIdTokenRequestCallbackKey);
}

}  // namespace content