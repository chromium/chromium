// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_PYTHON_UTILS_H_
#define NET_TEST_PYTHON_UTILS_H_

namespace base {
class CommandLine;
}

// Returns the command that should be used to launch Python 3.
[[nodiscard]] bool GetPython3Command(base::CommandLine* python_cmd);

#endif  // NET_TEST_PYTHON_UTILS_H_
