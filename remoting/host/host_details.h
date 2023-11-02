// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_DETAILS_H_
#define REMOTING_HOST_HOST_DETAILS_H_

#include <string>

namespace remoting {

// Returns the host OS name in a standard format for any build target.
std::string GetHostOperatingSystemName();

// Returns the host OS version in a standard format for any build target.
std::string GetHostOperatingSystemVersion();

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_DETAILS_H_
