// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_TERMS_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_TERMS_UTIL_H_

#include <string>

class GURL;

// Returns file name for Terms of Service text localized for the application
// locale.
std::string GetTermsOfServicePath();

GURL GetUnifiedTermsOfServiceURL(bool embedded);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_TERMS_UTIL_H_
