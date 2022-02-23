// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REDIRECT_URI_DATA_H_
#define CONTENT_BROWSER_WEBID_REDIRECT_URI_DATA_H_

#include <memory>
#include <string>

#include "base/supports_user_data.h"

namespace content {

class WebContents;

// This class holds on to the needed OpenID connect redirect callbacks to help
// connect the IDP response to the appropriate RP.
class RedirectUriData : public base::SupportsUserData::Data {
 public:
  explicit RedirectUriData(std::string redirect_uri);
  ~RedirectUriData() override;

  std::string Value();

  static void Set(WebContents* web_contents, std::string redirect_uri);
  static RedirectUriData* Get(WebContents* web_contents);
  static void Remove(WebContents* web_contents);

 private:
  std::string redirect_uri_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_WEBID_REDIRECT_URI_DATA_H_
