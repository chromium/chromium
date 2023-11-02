// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_MOJO_TEST_BLINK_EXPORT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_MOJO_TEST_BLINK_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_TEST_BLINK_IMPLEMENTATION)
#define MOJO_TEST_BLINK_EXPORT __declspec(dllexport)
#else
#define MOJO_TEST_BLINK_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_TEST_BLINK_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_TEST_BLINK_IMPLEMENTATION)
#define MOJO_TEST_BLINK_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_TEST_BLINK_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_TEST_BLINK_EXPORT
#endif

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_MOJO_TEST_BLINK_EXPORT_H_
