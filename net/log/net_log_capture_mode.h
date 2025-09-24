// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_CAPTURE_MODE_H_
#define NET_LOG_NET_LOG_CAPTURE_MODE_H_

#include <stdint.h>

#include <string>

#include "net/base/net_export.h"

class GURL;

namespace net {

// NetLogCaptureMode specifies the logging level.
//
// It is used to control which events are emitted to the log, and what level of
// detail is included in their parameters.
//
// The capture mode is expressed as a number, where higher values imply more
// information.
//
// Note the numeric values are used in a bitfield (NetLogCaptureModeSet) so must
// be sequential starting from 0, and not exceed 31.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum class NetLogCaptureMode : uint8_t {
  // Logging level that only allows fields that are known to be safe from a
  // privacy perspective.
  //
  // Logs recorded at this level are generally safe to share with untrusted
  // parties. They will not contain any information about IP addresses, host
  // names, URLs, headers, etc. They will still contain information about
  // timing, counts, and modes of operation (e.g. which version of HTTP is
  // used).
  //
  // Note this is implemented using a centrally managed allowlist of param keys;
  // see net_log_heavily_redacted_allowlist.h. Individual netlog entry producer
  // callsites do not have any say in this. This is by design to make it harder
  // for a given producer to accidentally log sensitive data.
  kHeavilyRedacted = 0,

  // Default logging level, which is expected to be light-weight and
  // does best-effort stripping of privacy/security sensitive data.
  //
  //  * Includes most HTTP request/response headers, but strips cookies and
  //    auth.
  //  * Does not include the full bytes read/written to sockets.
  kDefault,

  // Logging level that includes everything from kDefault, plus sensitive data
  // that it may have strippped.
  //
  //  * Includes cookies and authentication headers.
  //  * Does not include the full bytes read/written to sockets.
  kIncludeSensitive,

  // Logging level that includes everything that is possible to be logged.
  //
  //  * Includes the actual bytes read/written to sockets
  //  * Will result in large log files.
  kEverything,

  kLast = kEverything,
};

// Bitfield of NetLogCaptureMode, that should be initialized to zero for empty
// set. Bit "i" being set means that the set contains NetLogCaptureMode with
// value "i".
//
// Use the NetLogCaptureModeSet*() functions to operate on it.
using NetLogCaptureModeSet = uint8_t;

inline NetLogCaptureModeSet NetLogCaptureModeToBit(
    NetLogCaptureMode capture_mode) {
  return static_cast<uint8_t>(1 << static_cast<uint8_t>(capture_mode));
}

inline bool NetLogCaptureModeSetContains(NetLogCaptureMode capture_mode,
                                         NetLogCaptureModeSet set) {
  return (set & NetLogCaptureModeToBit(capture_mode)) != 0;
}

inline bool NetLogCaptureModeSetAdd(NetLogCaptureMode value,
                                    NetLogCaptureModeSet* set) {
  return *set |= NetLogCaptureModeToBit(value);
}

// Returns true if |capture_mode| permits logging sensitive values such as
// cookies and credentials.
NET_EXPORT bool NetLogCaptureIncludesSensitive(NetLogCaptureMode capture_mode);

// Returns true if |capture_mode| permits logging the full request/response
// bytes from sockets.
NET_EXPORT bool NetLogCaptureIncludesSocketBytes(
    NetLogCaptureMode capture_mode);

// Returns `url` as a string, with the username/password portions removed, if
// `capture_mode` mandates it. Always creates a copy of the passed in URL as a
// string, but since NetLog requires copies of strings be put in Value::Dicts
// anyways, using this method results in no extra copies over adding url.spec()
// to a dictionary directly.
//
// Should be used when logging the full input URL, which may contrain
// credentials. For layers that don't have access to it (e.g., anything below
// HttpNetworkTransaction), there's no need to use this function, though doing
// so should not significantly affect performance when logging.
NET_EXPORT std::string SanitizeUrlForNetLog(const GURL& url,
                                            NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_LOG_NET_LOG_CAPTURE_MODE_H_
