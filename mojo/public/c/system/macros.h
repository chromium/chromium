// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_MACROS_H_
#define MOJO_PUBLIC_C_SYSTEM_MACROS_H_

#include <stddef.h>
#include <stdint.h>

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

// Provides a convenient test for the presence of a field in a user-provided
// structure from a potentially older version of the ABI. Presence is determined
// by comparing the struct's provided |struct_size| value against the known
// offset and size of the field in this version of the ABI. Because fields are
// never reordered or removed, this is a sufficient test for the field's
// presence within whatever version of the ABI the client is programmed against.
#define MOJO_IS_STRUCT_FIELD_PRESENT(struct_pointer, field)    \
  ((size_t)(uintptr_t)((const char*)&(struct_pointer)->field - \
                       (const char*)(struct_pointer)) +        \
       sizeof((struct_pointer)->field) <=                      \
   (struct_pointer)->struct_size)

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
