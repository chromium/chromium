// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SECURITY_CAPABILITIES_H_
#define SANDBOX_WIN_SRC_SECURITY_CAPABILITIES_H_

#include <windows.h>

#include <vector>

#include "base/win/sid.h"

namespace sandbox {

class SecurityCapabilities final : public SECURITY_CAPABILITIES {
 public:
  explicit SecurityCapabilities(const base::win::Sid& package_sid);
  SecurityCapabilities(const base::win::Sid& package_sid,
                       const std::vector<base::win::Sid>& capabilities);

  SecurityCapabilities(const SecurityCapabilities&) = delete;
  SecurityCapabilities& operator=(const SecurityCapabilities&) = delete;

  ~SecurityCapabilities();

 private:
  std::vector<base::win::Sid> capabilities_;
  std::vector<SID_AND_ATTRIBUTES> capability_sids_;
  base::win::Sid package_sid_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SECURITY_CAPABILITIES_H_
