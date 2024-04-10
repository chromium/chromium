// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_ALLOWLIST_H_
#define HEADLESS_LIB_RENDERER_ALLOWLIST_H_

#include <string>
#include <vector>

#include "headless/public/headless_export.h"

namespace headless {

class HEADLESS_EXPORT Allowlist {
 public:
  Allowlist(std::string list, bool default_allow);
  ~Allowlist();

  Allowlist(const Allowlist&) = delete;
  Allowlist& operator=(const Allowlist&) = delete;

  bool IsAllowed(std::string_view entry) const;

 private:
  const std::string storage_;
  const bool default_allow_;
  const std::vector<std::string_view> entries_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_ALLOWLIST_H_
