// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_

#include "base/strings/string_piece_forward.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// This function returns whether the specified scheme is valid as a protocol
// handler parameter, as described in steps 1. and 2. of the HTML specification:
// https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
//
// The allow_ext_prefix parameter indicates whether the "ext+" prefix should be
// considered valid for custom schemes. This is to allow custom schemes that
// are reserved for browser extensions, similar to what Mozilla implements:
// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/protocol_handlers
//
// The out parameter has_custom_scheme_prefix is set to whether the scheme
// starts with a prefix indicating a custom scheme i.e. an ASCII case
// insensitive match to the string "web+" (or alternatively "ext+" if allowed).
bool BLINK_COMMON_EXPORT
IsValidCustomHandlerScheme(const base::StringPiece scheme,
                           bool allow_ext_prefix,
                           bool& has_custom_scheme_prefix);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_UTILS_H_
