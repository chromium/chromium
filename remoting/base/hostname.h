// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_HOSTNAME_H_
#define REMOTING_BASE_HOSTNAME_H_

#include <string>

namespace remoting {

// Returns the hostname (FQDN) of the machine.
extern std::string GetHostname();

}  // namespace remoting

#endif  // REMOTING_BASE_HOSTNAME_H_
