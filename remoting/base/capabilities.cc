// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/capabilities.h"

#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting {

bool HasCapability(const std::string& capabilities, const std::string& key) {
  std::vector<std::string_view> caps = base::SplitStringPiece(
      capabilities, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::Contains(caps, std::string_view(key));
}

std::string IntersectCapabilities(const std::string& client_capabilities,
                                  const std::string& host_capabilities) {
  std::vector<std::string_view> client_caps =
      base::SplitStringPiece(client_capabilities, " ", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  std::sort(client_caps.begin(), client_caps.end());

  std::vector<std::string_view> host_caps = base::SplitStringPiece(
      host_capabilities, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::sort(host_caps.begin(), host_caps.end());

  std::vector<std::string_view> result =
      base::STLSetIntersection<std::vector<std::string_view>>(client_caps,
                                                              host_caps);

  return base::JoinString(result, " ");
}

}  // namespace remoting
