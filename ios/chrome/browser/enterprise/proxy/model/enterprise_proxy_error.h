// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_H_

#import <Foundation/Foundation.h>

// Domain of the error that fails a main-frame navigation whose managed proxy
// auth challenge could not be answered with credentials. The web layer appends
// that error as the final underlying error of the navigation error, which is
// where `ChromeWebClient::PrepareErrorPage()` looks for it. The details of the
// failure are in the disguised error that `EnterpriseProxyErrorService` records
// under the navigation ID.
extern const NSErrorDomain kEnterpriseProxyErrorDomain;

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_H_
