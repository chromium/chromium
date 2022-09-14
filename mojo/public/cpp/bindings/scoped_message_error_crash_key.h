// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SCOPED_MESSAGE_ERROR_CRASH_KEY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SCOPED_MESSAGE_ERROR_CRASH_KEY_H_

#include <string>

#include "base/component_export.h"
#include "base/debug/crash_logging.h"

namespace mojo {
namespace debug {

// Helper class for storing |mojo_message_error| in the right crash key (when
// initiating a base::debug::DumpWithoutCrashing because of a bad message
// report).
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ScopedMessageErrorCrashKey
    : public base::debug::ScopedCrashKeyString {
 public:
  explicit ScopedMessageErrorCrashKey(const std::string& mojo_message_error);
  ~ScopedMessageErrorCrashKey();

  ScopedMessageErrorCrashKey(const ScopedMessageErrorCrashKey&) = delete;
  ScopedMessageErrorCrashKey& operator=(const ScopedMessageErrorCrashKey&) =
      delete;
};

}  // namespace debug
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SCOPED_MESSAGE_ERROR_CRASH_KEY_H_
