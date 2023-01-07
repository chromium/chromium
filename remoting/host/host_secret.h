// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SECRET_H_
#define REMOTING_HOST_HOST_SECRET_H_

#include <string>

namespace remoting {

// Generates random host secret.
std::string GenerateSupportHostSecret();

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SECRET_H_
