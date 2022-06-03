// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"

#include <memory>
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void ResourceTimingInfo::AddRedirect(const ResourceResponse& redirect_response,
                                     const KURL& new_url) {
  redirect_chain_.push_back(redirect_response);
}

}  // namespace blink
