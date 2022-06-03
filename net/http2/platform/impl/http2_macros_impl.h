// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_MACROS_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_MACROS_IMPL_H_

#include "base/check.h"
#include "base/compiler_specific.h"

#define HTTP2_FALLTHROUGH_IMPL FALLTHROUGH
#define HTTP2_UNREACHABLE_IMPL() DCHECK(false)
#define HTTP2_DIE_IF_NULL_IMPL(ptr) (ptr)

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_MACROS_IMPL_H_
