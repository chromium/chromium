// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_

namespace network {

// The default buffer size of DataPipe which is used to send the content body.
static constexpr size_t kDataPipeDefaultAllocationSize = 512 * 1024;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
