// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FQDN_H_
#define REMOTING_BASE_FQDN_H_

#include <string>

namespace remoting {

// Returns the FQDN (AKA "hostname" in net/base) of the machine.
extern std::string GetFqdn();

}  // namespace remoting

#endif  // REMOTING_BASE_FQDN_H_
