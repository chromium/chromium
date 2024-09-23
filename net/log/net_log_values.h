// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_VALUES_H_
#define NET_LOG_NET_LOG_VALUES_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/values.h"
#include "net/base/net_export.h"

namespace net {

// Helpers to construct dictionaries with a single key and value. Useful for
// building parameters to include in a NetLog.
NET_EXPORT base::Value::Dict NetLogParamsWithInt(std::string_view name,
                                                 int value);
NET_EXPORT base::Value::Dict NetLogParamsWithInt64(std::string_view name,
                                                   int64_t value);
NET_EXPORT base::Value::Dict NetLogParamsWithBool(std::string_view name,
                                                  bool value);
NET_EXPORT base::Value::Dict NetLogParamsWithString(std::string_view name,
                                                    std::string_view value);

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
NET_EXPORT base::Value NetLogStringValue(std::string_view raw);

// Creates a base::Value() to represent the octets |bytes|. This should be
// used when adding binary data (i.e. not an ASCII or UTF-8 string) to the
// NetLog. The resulting base::Value() holds a copy of the input data.
//
// This wrapper must be used rather than directly adding base::Value parameters
// of type BINARY to the NetLog, since the JSON writer does not support
// serializing them.
//
// This wrapper encodes |bytes| as a Base64 encoded string.
NET_EXPORT base::Value NetLogBinaryValue(base::span<const uint8_t> bytes);
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
NET_EXPORT base::Value NetLogNumberValue(uint32_t num);

}  // namespace net

#endif  // NET_LOG_NET_LOG_VALUES_H_
