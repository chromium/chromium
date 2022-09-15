// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_USERNAME_H_
#define REMOTING_HOST_BASE_USERNAME_H_

#include <string>

namespace remoting {

// Returns the username associated with this process, or the empty string on
// error or if not implemented.
std::string GetUsername();

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_USERNAME_H_
