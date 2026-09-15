// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state_test_util.h"

#include "net/http/transport_security_state.h"

namespace net {

namespace test_default {
// TODO(crbug.com/497882860): Remove pins includes from this file.
#include "net/http/transport_security_state_static_pins_unittest_default.h"
#include "net/http/transport_security_state_static_unittest_default.h"
}  // namespace test_default

ScopedTransportSecurityStateSource::ScopedTransportSecurityStateSource() {
  // TODO(mattm): allow using other source?
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
  // TODO(crbug.com/497882860): Remove/split out the setting of the pins
  // source from this scoper.
  SetTransportSecurityStatePinsSourceForTesting(&test_default::kPinsSource);
}

ScopedTransportSecurityStateSource::~ScopedTransportSecurityStateSource() {
  SetTransportSecurityStateSourceForTesting(nullptr);
  SetTransportSecurityStatePinsSourceForTesting(nullptr);
}

}  // namespace net
