// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KEY_PINNING_STATE_CHROME_DATA_H_
#define NET_CERT_KEY_PINNING_STATE_CHROME_DATA_H_

#include "base/time/time.h"

namespace net {

struct TransportSecurityStatePinsSource;

const TransportSecurityStatePinsSource* GetCompiledPinsSource();

base::Time GetCompiledPinsListTimestamp();

}  // namespace net

#endif  // NET_CERT_KEY_PINNING_STATE_CHROME_DATA_H_
