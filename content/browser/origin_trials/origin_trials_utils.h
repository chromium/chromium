// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_UTILS_H_
#define CONTENT_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_UTILS_H_

#include <string>
#include <vector>

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

// Get values of the "Origin-Trial" header.
std::vector<std::string> GetOriginTrialHeaderValues(
    const net::HttpResponseHeaders* headers);

// Get values of the "Critical-Origin-Trial" header.
std::vector<std::string> GetCriticalOriginTrialHeaderValues(
    const net::HttpResponseHeaders* headers);

}  // namespace content

#endif  // CONTENT_BROWSER_ORIGIN_TRIALS_ORIGIN_TRIALS_UTILS_H_
