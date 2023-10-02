// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_crash_message.h"

#include <stdint.h>

#include <string>

// SPI from CrashReporterClient.h

struct crashreporter_annotations_t {
  uint64_t version;
  const char* message;
  uint64_t signature_string;
  uint64_t backtrace;
  uint64_t message2;
  uint64_t thread;
  uint64_t dialog_mode;
  uint64_t abort_cause;
};

namespace {

crashreporter_annotations_t annotation
    __attribute__((section("__DATA,__crash_info"))) = {5, nullptr, 0, 0,
                                                       0, 0,       0, 0};

}

namespace sandbox::crash_message {

void SetCrashMessage(const char* message) {
  // Copy the message into a static string to ensure that the pointer stored in
  // gCRAnnotations remains valid.
  static std::string* eternal_crash_message = new std::string;
  eternal_crash_message->assign(message);
  annotation.message = eternal_crash_message->data();
}

}  // namespace sandbox::crash_message
