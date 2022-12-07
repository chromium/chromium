// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "net/http/transport_security_state_source.h"

namespace net {

class ScopedTransportSecurityStateSource {
 public:
  // Set the global transport security state preloaded static data source to
  // the transport_security_state_static_unittest_default source.
  ScopedTransportSecurityStateSource();

  // As above, but modifies the reporting URIs in the test source to have a
  // port number of |reporting_port|.
  explicit ScopedTransportSecurityStateSource(uint16_t reporting_port);

  ScopedTransportSecurityStateSource(
      const ScopedTransportSecurityStateSource&) = delete;
  ScopedTransportSecurityStateSource& operator=(
      const ScopedTransportSecurityStateSource&) = delete;

  ~ScopedTransportSecurityStateSource();

 private:
  std::unique_ptr<TransportSecurityStateSource> source_;

  // This data backs the members of |source_|, if they had to be modified to
  // use a different reporting port number.
  std::string pkp_report_uri_;
  std::vector<TransportSecurityStateSource::Pinset> pinsets_;
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_TEST_UTIL_H_
