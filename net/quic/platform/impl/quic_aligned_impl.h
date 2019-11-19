// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_ALIGNED_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_ALIGNED_IMPL_H_

#ifdef _MSC_VER
// MSVC 2013 and prior don't have alignof or aligned(); they have __alignof and
// a __declspec instead.
#define QUIC_ALIGN_OF_IMPL __alignof
#define QUIC_ALIGNED_IMPL(X) __declspec(align(X))
#else
#define QUIC_ALIGN_OF_IMPL alignof
#define QUIC_ALIGNED_IMPL(X) __attribute__((aligned(X)))
#endif  // _MSC_VER

// TODO(rtenneti): Change the default 64 alignas value (used the default
// value from ABSL_CACHELINE_SIZE).
#define QUIC_CACHELINE_SIZE_IMPL (64)
#define QUIC_CACHELINE_ALIGNED_IMPL ALIGNAS(QUIC_CACHELINE_SIZE)

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_ALIGNED_IMPL_H_
