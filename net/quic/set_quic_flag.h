// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_SET_QUIC_FLAG_H_
#define NET_QUIC_SET_QUIC_FLAG_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

// Sets the flag named |flag_name| to the value of |value| after converting
// it from a string to the appropriate type. If |value| is invalid or out of
// range, the flag will be unchanged.
NET_EXPORT_PRIVATE void SetQuicFlagByName(const std::string& flag_name,
                                          const std::string& value);

}  // namespace net

#endif  // NET_QUIC_SET_QUIC_FLAG_H_
