// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_ATTRIBUTES_H_
#define REMOTING_HOST_HOST_ATTRIBUTES_H_

#include <string>
#include <vector>

namespace remoting {

// Returns the list of host attribute tokens for the current process/system. The
// result may vary if any system configurations change, so consumers should not
// cache the result.
// This function is thread-safe.
// Note, this is not a cheap call on Windows: it spawns a new process to
// retrieve some of the attributes.
std::vector<std::string> GetHostAttributes();

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_ATTRIBUTES_H_
