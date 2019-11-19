// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_VALUES_H_
#define NET_LOG_NET_LOG_VALUES_H_

#include <stdint.h>

#include "base/strings/string_piece_forward.h"
#include "net/base/net_export.h"

namespace base {
class Value;
}

namespace net {

// Helpers to construct dictionaries with a single key and value. Useful for
// building parameters to include in a NetLog.
NET_EXPORT base::Value NetLogParamsWithInt(base::StringPiece name, int value);
NET_EXPORT base::Value NetLogParamsWithInt64(base::StringPiece name,
                                             int64_t value);
NET_EXPORT base::Value NetLogParamsWithBool(base::StringPiece name, bool value);
NET_EXPORT base::Value NetLogParamsWithString(base::StringPiece name,
                                              base::StringPiece value);

// Creates a base::Value() to represent the byte string |raw| when adding it to
// the NetLog.
//
// When |raw| is an ASCII string, the returned value is a base::Value()
// containing that exact string. Otherwise it is represented by a
// percent-escaped version of the original string, along with a special prefix.
//
// This wrapper exists because base::Value strings are required to be UTF-8.
// Often times NetLog consumers just want to log a std::string, and that string
// may not be UTF-8.
NET_EXPORT base::Value NetLogStringValue(base::StringPiece raw);

// Creates a base::Value() to represent the octets |bytes|. This should be
// used when adding binary data (i.e. not an ASCII or UTF-8 string) to the
// NetLog. The resulting base::Value() holds a copy of the input data.
//
// This wrapper must be used rather than directly adding base::Value parameters
// of type BINARY to the NetLog, since the JSON writer does not support
// serializing them.
//
// This wrapper encodes |bytes| as a Base64 encoded string.
NET_EXPORT base::Value NetLogBinaryValue(const void* bytes, size_t length);

// Creates a base::Value() to represent integers, including 64-bit ones.
// base::Value() does not directly support 64-bit integers, as it is not
// representable in JSON.
//
// These wrappers will return values that are either numbers, or a string
// representation of their decimal value, depending on what is needed to ensure
// no loss of precision when de-serializing from JavaScript.
NET_EXPORT base::Value NetLogNumberValue(int64_t num);
NET_EXPORT base::Value NetLogNumberValue(uint64_t num);

}  // namespace net

#endif  // NET_LOG_NET_LOG_VALUES_H_
