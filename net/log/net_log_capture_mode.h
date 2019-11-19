// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_CAPTURE_MODE_H_
#define NET_LOG_NET_LOG_CAPTURE_MODE_H_

#include <stdint.h>

#include "net/base/net_export.h"

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
enum class NetLogCaptureMode : uint32_t {
  // Default logging level, which is expected to be light-weight and
  // does best-effort stripping of privacy/security sensitive data.
  //
  //  * Includes most HTTP request/response headers, but strips cookies and
  //    auth.
  //  * Does not include the full bytes read/written to sockets.
  kDefault = 0,

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
using NetLogCaptureModeSet = uint32_t;

inline NetLogCaptureModeSet NetLogCaptureModeToBit(
    NetLogCaptureMode capture_mode) {
  return 1 << static_cast<uint32_t>(capture_mode);
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

}  // namespace net

#endif  // NET_LOG_NET_LOG_CAPTURE_MODE_H_
