// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_MACROS_H_
#define MOJO_PUBLIC_C_SYSTEM_MACROS_H_

#include <stddef.h>

#if !defined(__cplusplus)
#include <assert.h>    // Defines static_assert() in C11.
#include <stdalign.h>  // Defines alignof() in C11.
#endif

// Assert things at compile time. (|msg| should be a valid identifier name.)
// Use like:
//   MOJO_STATIC_ASSERT(sizeof(struct Foo) == 12, "Foo has invalid size");
#define MOJO_STATIC_ASSERT(expr, msg) static_assert(expr, msg)

// Defines a pointer-sized struct field of the given type. This ensures that the
// field has an 8-byte footprint on both 32-bit and 64-bit systems, using an
// anonymous bitfield of either 32 or 0 bits, depending on pointer size.
// clang-format off
#define MOJO_POINTER_FIELD(type, name) \
  type name;                           \
  uint32_t : (sizeof(void*) == 4 ? 32 : 0)
// clang-format on

// Like the C++11 |alignof| operator.
#define MOJO_ALIGNOF(type) alignof(type)

// Specify the alignment of a |struct|, etc.
// Use like:
//   struct MOJO_ALIGNAS(8) Foo { ... };
// Unlike the C++11 |alignas()|, |alignment| must be an integer. It may not be a
// type, nor can it be an expression like |MOJO_ALIGNOF(type)| (due to the
// non-C++11 MSVS version).
// This can't use alignas() in C11 mode because unlike the C++11 version the
// C11 version can't be used on struct declarations.
#if defined(__cplusplus)
#define MOJO_ALIGNAS(alignment) alignas(alignment)
#elif defined(__GNUC__)
#define MOJO_ALIGNAS(alignment) __attribute__((aligned(alignment)))
#elif defined(_MSC_VER)
#define MOJO_ALIGNAS(alignment) __declspec(align(alignment))
#else
#error "Please define MOJO_ALIGNAS() for your compiler."
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_MACROS_H_
