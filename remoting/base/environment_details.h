// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENVIRONMENT_DETAILS_H_
#define REMOTING_BASE_ENVIRONMENT_DETAILS_H_

#include <string>

namespace remoting {

// The build version of the compiled binary (e.g. "123.4.567.8").
std::string GetBuildVersion();

// Returns the OS name in a standard format for any build target.
std::string GetOperatingSystemName();

// Returns the OS version in a standard format for any build target.
std::string GetOperatingSystemVersion();

}  // namespace remoting

#endif  // REMOTING_BASE_ENVIRONMENT_DETAILS_H_
