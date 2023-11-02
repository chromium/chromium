// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"

namespace mojo {
namespace debug {

namespace {

base::debug::CrashKeyString* GetMojoMessageErrorCrashKey() {
  // The "mojo-message-error" name used below is recognized by Chrome crash
  // analysis services - please avoid changing the name if possible.
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "mojo-message-error", base::debug::CrashKeySize::Size256);
  return crash_key;
}

}  // namespace

ScopedMessageErrorCrashKey::ScopedMessageErrorCrashKey(
    const std::string& mojo_message_error)
    : base::debug::ScopedCrashKeyString(GetMojoMessageErrorCrashKey(),
                                        mojo_message_error) {}

ScopedMessageErrorCrashKey::~ScopedMessageErrorCrashKey() = default;

}  // namespace debug
}  // namespace mojo
