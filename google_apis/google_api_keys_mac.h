// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_

#include "base/component_export.h"
#include "base/strings/cstring_view.h"

namespace google_apis {

COMPONENT_EXPORT(GOOGLE_APIS)
std::string GetAPIKeyFromInfoPlist(base::cstring_view key_name);

}  // namespace google_apis

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_
