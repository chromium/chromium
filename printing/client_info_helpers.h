// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_CLIENT_INFO_HELPERS_H_
#define PRINTING_CLIENT_INFO_HELPERS_H_

#include <cstddef>
#include "base/component_export.h"
#include "printing/mojom/print.mojom-forward.h"

namespace printing {

// Maximum length limits for 'client-info' member attributes.
inline constexpr size_t kClientInfoMaxNameLength = 127;
inline constexpr size_t kClientInfoMaxPatchesLength = 255;
inline constexpr size_t kClientInfoMaxStringVersionLength = 127;
inline constexpr size_t kClientInfoMaxVersionLength = 64;

// Returns true if all members of `client_info` are valid.
// String members are considered valid if they match the regex [a-zA-Z0-9_.-]*
// and do not exceed the maximum length specified for the respective IPP member
// attribute. The `client_type` member is valid if it is equal to one of the
// enum values defined for the `client-type` IPP attribute.
COMPONENT_EXPORT(PRINTING)
bool ValidateClientInfoItem(const mojom::IppClientInfo& client_info);

}  // namespace printing

#endif  // PRINTING_CLIENT_INFO_HELPERS_H_
