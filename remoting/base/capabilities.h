// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CAPABILITIES_H_
#define REMOTING_BASE_CAPABILITIES_H_

#include <string>

namespace remoting {

// Returns true if |capabilities| contain capability |key|.
bool HasCapability(const std::string& capabilities, const std::string& key);

// Returns a set of capabilities contained in both |client_capabilities| and
// |host_capabilities| sets.
std::string IntersectCapabilities(const std::string& client_capabilities,
                                  const std::string& host_capabilities);

}  // namespace remoting

#endif  // REMOTING_BASE_CAPABILITIES_H_
