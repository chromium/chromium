// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SIGNED_TREE_HEAD_H_
#define NET_CERT_SIGNED_TREE_HEAD_H_

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net::ct {

static const uint8_t kSthRootHashLength = 32;

// Signed Tree Head as defined in section 3.5. of RFC6962
struct NET_EXPORT SignedTreeHead {
  // Version enum in RFC 6962, Section 3.2. Note that while in the current
  // RFC the STH and SCT share the versioning scheme, there are plans in
  // RFC6962-bis to use separate versions, so using a separate scheme here.
  enum Version { V1 = 0, };

  SignedTreeHead();
  SignedTreeHead(Version version,
                 const base::Time& timestamp,
                 uint64_t tree_size,
                 const char sha256_root_hash[kSthRootHashLength],
                 const DigitallySigned& signature,
                 const std::string& log_id);
  SignedTreeHead(const SignedTreeHead& other);
  ~SignedTreeHead();

  Version version;
  base::Time timestamp;
  uint64_t tree_size;
  char sha256_root_hash[kSthRootHashLength];
  DigitallySigned signature;

  // Added in RFC6962-bis, Appendix A. Needed to identify which log
  // this STH belongs to.
  std::string log_id;
};

NET_EXPORT void PrintTo(const SignedTreeHead& sth, std::ostream* os);

NET_EXPORT bool operator==(const SignedTreeHead& lhs,
                           const SignedTreeHead& rhs);
NET_EXPORT bool operator!=(const SignedTreeHead& lhs,
                           const SignedTreeHead& rhs);

}  // namespace net::ct

#endif  // NET_CERT_SIGNED_TREE_HEAD_H_
