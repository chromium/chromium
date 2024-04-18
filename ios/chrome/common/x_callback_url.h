// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_X_CALLBACK_URL_H_
#define IOS_CHROME_COMMON_X_CALLBACK_URL_H_

#include <map>
#include <string>
#include <string_view>

#include "url/gurl.h"

// Returns true if `url` is compliant with the x-callback-url specs.
bool IsXCallbackURL(const GURL& url);

// Returns a GURL compliant with the x-callback-url specs (simple version with
// no parameters set, see XCallbackURLWithParameters for constructing complex
// URLs).
GURL CreateXCallbackURL(std::string_view scheme, std::string_view action);

// Returns a GURL compliant with the x-callback-url specs.
// See http://x-callback-url.com/specifications/ for specifications.
// `scheme` must not be empty, all other parameters may be.
GURL CreateXCallbackURLWithParameters(
    std::string_view scheme,
    std::string_view action,
    const GURL& success_url,
    const GURL& error_url,
    const GURL& cancel_url,
    const std::map<std::string, std::string>& parameters);

// Extract query parameters from an x-callback-url URL. `x_callback_url` must
// be compliant with the x-callback-url specs.
std::map<std::string, std::string> ExtractQueryParametersFromXCallbackURL(
    const GURL& x_callback_url);

#endif  // IOS_CHROME_COMMON_X_CALLBACK_URL_H_
