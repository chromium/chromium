// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_

#include "base/strings/string_piece_forward.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

bool BLINK_COMMON_EXPORT
IsValidCustomHandlerScheme(const base::StringPiece scheme,
                           bool allow_ext_prefix,
                           bool& has_custom_scheme_prefix);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_
