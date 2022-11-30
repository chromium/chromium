// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_SYSTEM_IMPL_EXPORT_H_
#define MOJO_CORE_SYSTEM_IMPL_EXPORT_H_

#if defined(MOJO_CORE_SHARED_LIBRARY)
#define MOJO_SYSTEM_IMPL_EXPORT
#else
#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_SYSTEM_IMPL_IMPLEMENTATION)
#define MOJO_SYSTEM_IMPL_EXPORT __declspec(dllexport)
#else
#define MOJO_SYSTEM_IMPL_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_SYSTEM_IMPL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_SYSTEM_IMPL_IMPLEMENTATION)
#define MOJO_SYSTEM_IMPL_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_SYSTEM_IMPL_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_SYSTEM_IMPL_EXPORT
#endif
#endif

#endif  // MOJO_CORE_SYSTEM_IMPL_EXPORT_H_
