// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_NAMES_H_
#define MEDIA_BASE_KEY_SYSTEM_NAMES_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// TODO(jrummell): This file should be folded into key_systems.cc as that is
// the primary user of these functions. http://crbug.com/606579.

// Returns true if |key_system| is Clear Key, false otherwise.
MEDIA_EXPORT bool IsClearKey(const std::string& key_system);

// Returns true if |key_system| is (reverse) sub-domain of |base|.
MEDIA_EXPORT bool IsSubKeySystemOf(const std::string& key_system,
                                   const std::string& base);

// Returns true if |key_system| is External Clear Key, false otherwise.
MEDIA_EXPORT bool IsExternalClearKey(const std::string& key_system);

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_NAMES_H_
