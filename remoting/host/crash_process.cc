// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/crash_process.h"

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace remoting {

void CrashProcess(const base::Location& location) {
  CrashProcess(location.function_name(), location.file_name(),
               location.line_number());
}

void CrashProcess(const std::string& function_name,
                  const std::string& file_name,
                  int line_number) {
  char message[1024];
  base::snprintf(message, sizeof(message), "Requested by %s at %s, line %d.",
                 function_name.c_str(), file_name.c_str(), line_number);
  base::debug::Alias(message);

  // Crash the process.
  CHECK(false) << message;
}

}  // namespace remoting
