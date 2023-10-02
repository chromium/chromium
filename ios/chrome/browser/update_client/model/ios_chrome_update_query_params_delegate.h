// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPDATE_CLIENT_MODEL_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UPDATE_CLIENT_MODEL_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "components/update_client/update_query_params_delegate.h"

class IOSChromeUpdateQueryParamsDelegate
    : public update_client::UpdateQueryParamsDelegate {
 public:
  IOSChromeUpdateQueryParamsDelegate();

  IOSChromeUpdateQueryParamsDelegate(
      const IOSChromeUpdateQueryParamsDelegate&) = delete;
  IOSChromeUpdateQueryParamsDelegate& operator=(
      const IOSChromeUpdateQueryParamsDelegate&) = delete;

  ~IOSChromeUpdateQueryParamsDelegate() override;

  // Gets the instance for IOSChromeUpdateQueryParamsDelegate.
  static IOSChromeUpdateQueryParamsDelegate* GetInstance();

  // update_client::UpdateQueryParamsDelegate:
  std::string GetExtraParams() override;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  static const std::string& GetLang();
};

#endif  // IOS_CHROME_BROWSER_UPDATE_CLIENT_MODEL_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
