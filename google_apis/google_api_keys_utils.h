// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_UTILS_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_UTILS_H_

#include <string>

#include "base/component_export.h"

namespace google_apis {

// Returns an API key that can be used to override the API key and that is
// configured via an experimental feature.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string GetAPIKeyOverrideViaFeature();

COMPONENT_EXPORT(GOOGLE_APIS)
void LogAPIKeysMatchHistogram(bool match);

}  // namespace google_apis

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_UTILS_H_
