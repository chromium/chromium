// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/user_agent.h"

#include "content/public/common/user_agent.h"

namespace headless {

std::string BuildUserAgentFromProduct(const std::string& product) {
  return content::BuildUserAgentFromProduct(product);
}

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  return content::BuildUserAgentFromOSAndProduct(os_info, product);
}

}  // namespace headless
