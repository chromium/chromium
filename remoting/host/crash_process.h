// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CRASH_PROCESS_H_
#define REMOTING_HOST_CRASH_PROCESS_H_

#include <string>

namespace base {
class Location;
}

namespace remoting {

void CrashProcess(const base::Location& location);

void CrashProcess(const std::string& function_name,
                  const std::string& file_name,
                  int line_number);

}  // namespace remoting

#endif  // REMOTING_HOST_CRASH_PROCESS_H_
