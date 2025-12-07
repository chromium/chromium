// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/session/proto/proto_test_util.h"

namespace web::proto {
namespace {

// Equality operator for `google::protobuf::RepeatedPtrField<T>` for use
// in tests.
template <typename T>
bool operator==(const google::protobuf::RepeatedPtrField<T>& lhs,
                const google::protobuf::RepeatedPtrField<T>& rhs) {
  auto lhs_iter = lhs.begin();
  auto rhs_iter = rhs.begin();
  const auto lhs_end = lhs.end();
  const auto rhs_end = rhs.end();
  for (; lhs_iter != lhs_end && rhs_iter != rhs_end; ++lhs_iter, ++rhs_iter) {
    if (*lhs_iter != *rhs_iter) {
      return false;
    }
  }

  return lhs_iter == lhs_end && rhs_iter == rhs_end;
}

// Inequality operator for `google::protobuf::RepeatedPtrField<T>` for use
// in tests.
template <typename T>
bool operator!=(const google::protobuf::RepeatedPtrField<T>& lhs,
                const google::protobuf::RepeatedPtrField<T>& rhs) {
  return !(lhs == rhs);
}

}  // anonymous namespace

bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
  if (lhs.microseconds() != rhs.microseconds()) {
    return false;
  }

  return true;
}

bool operator!=(const Timestamp& lhs, const Timestamp& rhs) {
  return !(lhs == rhs);
}

bool operator==(const ReferrerStorage& lhs, const ReferrerStorage& rhs) {
  if (lhs.url() != rhs.url()) {
    return false;
  }

  if (lhs.policy() != rhs.policy()) {
    return false;
  }

  return true;
}

bool operator!=(const ReferrerStorage& lhs, const ReferrerStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const HttpHeaderStorage& lhs, const HttpHeaderStorage& rhs) {
  if (lhs.name() != rhs.name()) {
    return false;
  }

  if (lhs.value() != rhs.value()) {
    return false;
  }

  return true;
}

bool operator!=(const HttpHeaderStorage& lhs, const HttpHeaderStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const HttpHeaderListStorage& lhs,
                const HttpHeaderListStorage& rhs) {
  return lhs.headers() == rhs.headers();
}

bool operator!=(const HttpHeaderListStorage& lhs,
                const HttpHeaderListStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const NavigationItemStorage& lhs,
                const NavigationItemStorage& rhs) {
  if (lhs.url() != rhs.url()) {
    return false;
  }

  if (lhs.virtual_url() != rhs.virtual_url()) {
    return false;
  }

  if (lhs.title() != rhs.title()) {
    return false;
  }

  if (lhs.timestamp() != rhs.timestamp()) {
    return false;
  }

  if (lhs.user_agent() != rhs.user_agent()) {
    return false;
  }

  if (lhs.referrer() != rhs.referrer()) {
    return false;
  }

  if (lhs.http_request_headers() != rhs.http_request_headers()) {
    return false;
  }

  return true;
}

bool operator!=(const NavigationItemStorage& lhs,
                const NavigationItemStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const NavigationStorage& lhs, const NavigationStorage& rhs) {
  if (lhs.last_committed_item_index() != rhs.last_committed_item_index()) {
    return false;
  }

  if (lhs.items() != rhs.items()) {
    return false;
  }

  return true;
}

bool operator!=(const NavigationStorage& lhs, const NavigationStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const CertificateStorage& lhs, const CertificateStorage& rhs) {
  if (lhs.certificate() != rhs.certificate()) {
    return false;
  }

  if (lhs.host() != rhs.host()) {
    return false;
  }

  if (lhs.status() != rhs.status()) {
    return false;
  }

  return true;
}

bool operator!=(const CertificateStorage& lhs, const CertificateStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const CertificatesCacheStorage& lhs,
                const CertificatesCacheStorage& rhs) {
  return lhs.certs() == rhs.certs();
}

bool operator!=(const CertificatesCacheStorage& lhs,
                const CertificatesCacheStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const WebStateStorage& lhs, const WebStateStorage& rhs) {
  if (lhs.has_opener() != rhs.has_opener()) {
    return false;
  }

  if (lhs.user_agent() != rhs.user_agent()) {
    return false;
  }

  if (lhs.certs_cache() != rhs.certs_cache()) {
    return false;
  }

  if (lhs.navigation() != rhs.navigation()) {
    return false;
  }

  return true;
}

bool operator!=(const WebStateStorage& lhs, const WebStateStorage& rhs) {
  return !(lhs == rhs);
}

}  // namespace web::proto
