// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_

#include <string>

#include "base/component_export.h"

namespace google_apis {

COMPONENT_EXPORT(GOOGLE_APIS)
std::string GetAPIKeyFromInfoPlist(const std::string& key_name);

}  // namespace google_apis

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_MAC_H_
