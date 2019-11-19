// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DER_ENCODE_VALUES_H_
#define NET_DER_ENCODE_VALUES_H_

#include <stddef.h>
#include <stdint.h>

#include "net/base/net_export.h"

namespace base {
class Time;
}

namespace net {

namespace der {

struct GeneralizedTime;

// Encodes |time|, a UTC-based time, to DER |generalized_time|, for comparing
// against other GeneralizedTime objects.
NET_EXPORT bool EncodeTimeAsGeneralizedTime(const base::Time& time,
                                            GeneralizedTime* generalized_time);

// Converts a GeneralizedTime struct to a base::Time, returning true on success
// or false if |generalized| was invalid or cannot be represented by
// base::Time.
NET_EXPORT bool GeneralizedTimeToTime(const der::GeneralizedTime& generalized,
                                      base::Time* result);

static const size_t kGeneralizedTimeLength = 15;

// Encodes |time| to |out| as a DER GeneralizedTime value. Returns true on
// success and false on error.
NET_EXPORT bool EncodeGeneralizedTime(const GeneralizedTime& time,
                                      uint8_t out[kGeneralizedTimeLength]);

static const size_t kUTCTimeLength = 13;

// Encodes |time| to |out| as a DER UTCTime value. Returns true on success and
// false on error.
NET_EXPORT bool EncodeUTCTime(const GeneralizedTime& time,
                              uint8_t out[kUTCTimeLength]);

}  // namespace der

}  // namespace net

#endif  // NET_DER_ENCODE_VALUES_H_
