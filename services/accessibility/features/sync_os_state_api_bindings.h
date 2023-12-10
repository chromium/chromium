// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_SYNC_OS_STATE_API_BINDINGS_H_
#define SERVICES_ACCESSIBILITY_FEATURES_SYNC_OS_STATE_API_BINDINGS_H_

#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-value.h"

namespace ax {

// Gets the display name for the provided locale in the display_locale.
std::string GetDisplayNameForLocale(const std::string& locale,
                                    const std::string& display_locale);

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_SYNC_OS_STATE_API_BINDINGS_H_
