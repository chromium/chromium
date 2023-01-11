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
using CompletionRepeatingCallback = base::RepeatingCallback<void(int)>;

// 64bit version of the RepeatingCallback specialization that takes a single
// int64_t parameter. Usually this is used to report a file offset, size or
// network error code.
using Int64CompletionRepeatingCallback = base::RepeatingCallback<void(int64_t)>;

using CancelableCompletionRepeatingCallback =
    base::CancelableRepeatingCallback<void(int)>;

}  // namespace net

#endif  // NET_BASE_COMPLETION_REPEATING_CALLBACK_H_
