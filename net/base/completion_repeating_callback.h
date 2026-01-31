// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_REPEATING_CALLBACK_H_
#define NET_BASE_COMPLETION_REPEATING_CALLBACK_H_

#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"

namespace net {

// A RepeatingCallback specialization that takes a single int parameter. Usually
// this is used to report a byte count or network error code.
//
// DEPRECATED, DO NOT USE! https://crbug.com/471017624
// Use the correct type, whether it be net::Error directly, or base::ByteSize
// for byte sizes, or base::expected for combining both into one result.
using CompletionRepeatingCallback = base::RepeatingCallback<void(int)>;

}  // namespace net

#endif  // NET_BASE_COMPLETION_REPEATING_CALLBACK_H_
