// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_ID_TOKEN_REQUEST_CALLBACK_DATA_H_
#define CONTENT_BROWSER_WEBID_ID_TOKEN_REQUEST_CALLBACK_DATA_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

using DoneCallback = base::OnceCallback<void(const std::string&)>;

// This class holds on to the needed WebID callbacks to help connect the IDP
// `navigator.id.provide()` response to the appropriate RP `navigator.id.get()`
// request.
//
// Note that the |web_contents| instance that is passed in must be that of the
// IDP sign-in page. An important assumption here is that this WebContents
// instance is used only for a single `navigator.id.get()` call. Otherwise the
// simple mechanism to use UserData would lead to callbacks from different WebID
// consumers overriding each leading to hard-to-detect breakage. Currently this
// assumption is valid since we create a new IDP sign-in window for each request
// and don't try to re-use them.
class CONTENT_EXPORT IdTokenRequestCallbackData
    : public base::SupportsUserData::Data {
 public:
  explicit IdTokenRequestCallbackData(DoneCallback callback);
  ~IdTokenRequestCallbackData() override;

  DoneCallback TakeDoneCallback();

  static void Set(WebContents* web_contents, DoneCallback callback);
  static IdTokenRequestCallbackData* Get(WebContents* web_contents);
  static void Remove(WebContents* web_contents);

 private:
  DoneCallback callback_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_WEBID_ID_TOKEN_REQUEST_CALLBACK_DATA_H_