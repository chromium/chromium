// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SANDBOX_EXPORT_H_
#define SANDBOX_SANDBOX_EXPORT_H_

#if defined(WIN32)
#error "sandbox_export.h does not support WIN32."
#endif

#if defined(COMPONENT_BUILD)

#if defined(SANDBOX_IMPLEMENTATION)
#define SANDBOX_EXPORT __attribute__((visibility("default")))
#else
#define SANDBOX_EXPORT
#endif  // defined(SANDBOX_IMPLEMENTATION)

#else  // defined(COMPONENT_BUILD)

#define SANDBOX_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // SANDBOX_SANDBOX_EXPORT_H_
