// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FQDN_H_
#define REMOTING_BASE_FQDN_H_

#include <string>

#include "build/build_config.h"

namespace remoting {

// Returns the FQDN (AKA "hostname" in net/base) of the machine.
extern std::string GetFqdn();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Returns a string that can be used to construct a host config file name, e.g.
// "host#1234567890aabbccddeeff1234567890".
// DEPRECATED: This should only be used for the single-process host for
// compatibility reasons. New config/setting files should not have the host hash
// in the filename.
std::string GetHostHash();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace remoting

#endif  // REMOTING_BASE_FQDN_H_
