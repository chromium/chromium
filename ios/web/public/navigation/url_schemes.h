// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_URL_SCHEMES_H_
#define IOS_WEB_PUBLIC_NAVIGATION_URL_SCHEMES_H_

namespace web {

// Note: WebMainRunner calls this method internally as part of main
// initialization, so this function generally should not be called
// by embedders. It's exported to facilitate test harnesses that do
// not utilize WebMainRunner and that do not wish to lock the set of
// standard schemes at init time.
//
// Called near the beginning of startup to register URL schemes that should be
// parsed as "standard" or "secure" with the src/url/ library. The set of
// schemes is then locked, disallowing further modification.
// The embedder can add additional schemes by overriding the
// WebClient::AddAdditionalSchemes method.
void RegisterWebSchemes();

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_URL_SCHEMES_H_
