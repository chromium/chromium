// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_API_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_API_H_

#include "base/macros.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class DisplaySourceGetAvailableSinksFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("displaySource.getAvailableSinks",
                             DISPLAYSOURCE_GETAVAILABLESINKS)
  DisplaySourceGetAvailableSinksFunction() = default;

 protected:
  ~DisplaySourceGetAvailableSinksFunction() override;
  ResponseAction Run() final;

 private:
  void OnGetSinksCompleted(const DisplaySourceSinkInfoList& sinks);
  void OnGetSinksFailed(const std::string& reason);

  DISALLOW_COPY_AND_ASSIGN(DisplaySourceGetAvailableSinksFunction);
};

class DisplaySourceRequestAuthenticationFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("displaySource.requestAuthentication",
                             DISPLAYSOURCE_REQUESTAUTHENTICATION)
  DisplaySourceRequestAuthenticationFunction() = default;

 protected:
  ~DisplaySourceRequestAuthenticationFunction() override;
  ResponseAction Run() final;

 private:
  void OnRequestAuthCompleted(const DisplaySourceAuthInfo& auth_info);
  void OnRequestAuthFailed(const std::string& reason);

  DISALLOW_COPY_AND_ASSIGN(DisplaySourceRequestAuthenticationFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_API_H_
