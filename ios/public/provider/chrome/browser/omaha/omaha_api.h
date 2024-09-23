// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_API_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ios {
namespace provider {

// Callback used to set attribute in Omaha request.
using AttributeSetter =
    base::RepeatingCallback<void(const std::string&, const std::string&)>;

// Returns the URL for the update checks. If the returned URL is invalid,
// omaha is not enabled.
GURL GetOmahaUpdateServerURL();

// Returns the unique ID for this application.
std::string GetOmahaApplicationId();

// Allows setting extra attributes in the omaha request. This function can be
// called multiple time per request. Only the attributes relevant for `element`
// should be set.
void SetOmahaExtraAttributes(std::string_view element, AttributeSetter setter);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_API_H_
