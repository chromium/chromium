// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_CLIENT_INFO_HELPERS_H_
#define PRINTING_CLIENT_INFO_HELPERS_H_

#include <string>

#include "base/component_export.h"
#include "printing/mojom/print.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

// Maximum length limits for 'client-info' member attributes.
inline constexpr size_t kClientInfoMaxNameLength = 127;
inline constexpr size_t kClientInfoMaxPatchesLength = 255;
inline constexpr size_t kClientInfoMaxStringVersionLength = 127;
inline constexpr size_t kClientInfoMaxVersionLength = 64;

// Returns the string representation of `client_info` in a format suitable for
// use as a `cups_option_t` value, or absl::nullopt if `client_info` is invalid.
// `client_info` represents one value of the 'client-info' multi-valued IPP
// attribute. `client_info` is considered valid if all string members match the
// regex [a-zA-Z0-9_.-]* and do not exceed the maximum length specified for the
// respective IPP member attribute.
COMPONENT_EXPORT(PRINTING)
absl::optional<std::string> ClientInfoCollectionToCupsOptionValue(
    const mojom::IppClientInfo& client_info);

}  // namespace printing

#endif  // PRINTING_CLIENT_INFO_HELPERS_H_
