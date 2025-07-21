// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_

#include "net/http/transport_security_state_source.h"

namespace net {

class ScopedTransportSecurityStateSource {
 public:
  // Set the global transport security state preloaded static data source to
  // the transport_security_state_static_unittest_default source.
  ScopedTransportSecurityStateSource();

  // Deprecated interface, now does the same as the default constructor.
  // TODO(crbug.com/41485778): remove this.
  explicit ScopedTransportSecurityStateSource(uint16_t reporting_port);

  ScopedTransportSecurityStateSource(
      const ScopedTransportSecurityStateSource&) = delete;
  ScopedTransportSecurityStateSource& operator=(
      const ScopedTransportSecurityStateSource&) = delete;

  ~ScopedTransportSecurityStateSource();
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_
