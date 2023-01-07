// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_USER_AGENT_H_
#define HEADLESS_PUBLIC_UTIL_USER_AGENT_H_

#include <string>

namespace headless {

std::string BuildUserAgentFromProduct(const std::string& product);
std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_USER_AGENT_H_
