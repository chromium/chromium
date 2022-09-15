// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"

namespace network {
namespace debug {

COMPONENT_EXPORT(NETWORK_CPP_CRASH_KEYS)
void SetDeserializationCrashKeyString(base::StringPiece str);

COMPONENT_EXPORT(NETWORK_CPP_CRASH_KEYS)
void ClearDeserializationCrashKeyString();

}  // namespace debug
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_
