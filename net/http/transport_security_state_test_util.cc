// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state_test_util.h"

#include "net/http/transport_security_state.h"

namespace net {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}  // namespace test_default

ScopedTransportSecurityStateSource::ScopedTransportSecurityStateSource() {
  // TODO(mattm): allow using other source?
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
}

ScopedTransportSecurityStateSource::~ScopedTransportSecurityStateSource() {
  SetTransportSecurityStateSourceForTesting(nullptr);
}

}  // namespace net
