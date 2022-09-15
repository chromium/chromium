// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_TEST_STRINGS_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_TEST_STRINGS_H_

namespace tracing {

namespace loader_lock_sampler_test {

// wchar_t is used because this is included by loader_lock_sampler_test_dll.cc,
// which calls native Windows APIs.
extern const wchar_t kWaitForLockEventName[];
extern const wchar_t kDropLockEventName[];

}  // namespace loader_lock_sampler_test

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_TEST_STRINGS_H_
