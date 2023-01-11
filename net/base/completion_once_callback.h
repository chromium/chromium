// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_ONCE_CALLBACK_H_
#define NET_BASE_COMPLETION_ONCE_CALLBACK_H_

#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"

namespace net {

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
using CompletionOnceCallback = base::OnceCallback<void(int)>;

// 64bit version of the OnceCallback specialization that takes a single int64_t
// parameter. Usually this is used to report a file offset, size or network
// error code.
using Int64CompletionOnceCallback = base::OnceCallback<void(int64_t)>;

using CancelableCompletionOnceCallback =
    base::CancelableOnceCallback<void(int)>;

}  // namespace net

#endif  // NET_BASE_COMPLETION_ONCE_CALLBACK_H_
