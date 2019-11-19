// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_PYTHON_UTILS_H_
#define NET_TEST_PYTHON_UTILS_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/environment.h"

namespace base {
class CommandLine;
class FilePath;
}

// Modifies |map| to use the specified Python path.
void SetPythonPathInEnvironment(const std::vector<base::FilePath>& python_path,
                                base::EnvironmentMap* map);

// Return the location of the compiler-generated python protobuf.
bool GetPyProtoPath(base::FilePath* dir);

// Returns if a virtualenv is currently active.
bool IsInPythonVirtualEnv();

// Returns the command that should be used to launch Python.
bool GetPythonCommand(base::CommandLine* python_cmd) WARN_UNUSED_RESULT;

#endif  // NET_TEST_PYTHON_UTILS_H_
