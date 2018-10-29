// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PLATFORM_DETAILS_H_
#define REMOTING_BASE_PLATFORM_DETAILS_H_

#include <string>

namespace remoting {

// Returns the OS version in a standard format for any build target.
std::string GetOperatingSystemVersionString();

}  // namespace remoting

#endif  // REMOTING_BASE_PLATFORM_DETAILS_H_
