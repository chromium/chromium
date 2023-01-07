// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_

#include <stddef.h>

#include "base/component_export.h"

namespace network {

// The default Accept header value to use if none were specified.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kDefaultAcceptHeaderValue[];

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
