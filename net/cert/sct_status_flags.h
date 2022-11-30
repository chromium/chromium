// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SCT_STATUS_FLAGS_H_
#define NET_CERT_SCT_STATUS_FLAGS_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net::ct {

// The possible verification statuses for a SignedCertificateTimestamp.
// Note: The numeric values are used within histograms and should not change
// or be re-assigned.
enum SCTVerifyStatus : uint32_t {
  // Not a real status, this just prevents a default int value from being
  // mis-interpreseted as a valid status.
  // Also used to count SCTs that cannot be decoded in the histogram.
  SCT_STATUS_NONE = 0,

  // The SCT is from an unknown log, so we cannot verify its signature.
  SCT_STATUS_LOG_UNKNOWN = 1,

  // Obsolete. Kept here to avoid reuse.
  // SCT_STATUS_INVALID = 2,

  // The SCT is from a known log, and the signature is valid.
  SCT_STATUS_OK = 3,

  // The SCT is from a known log, but the signature is invalid.
  SCT_STATUS_INVALID_SIGNATURE = 4,

  // The SCT is from a known log, but the timestamp is in the future.
  SCT_STATUS_INVALID_TIMESTAMP = 5,

  // Used to bound the enum values. Since this enum is passed over IPC,
  // the last value must be a valid one (rather than one past a valid one).
  SCT_STATUS_MAX = SCT_STATUS_INVALID_TIMESTAMP,
};

// Returns true if |status| denotes a valid value in SCTVerifyStatus, which
// is all current values in the enum except SCT_STATUS_NONE.
NET_EXPORT bool IsValidSCTStatus(uint32_t status);

}  // namespace net::ct

#endif  // NET_CERT_SCT_STATUS_FLAGS_H_
