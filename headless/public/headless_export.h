// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_EXPORT_H_
#define HEADLESS_PUBLIC_HEADLESS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(HEADLESS_IMPLEMENTATION)
#define HEADLESS_EXPORT __declspec(dllexport)
#else
#define HEADLESS_EXPORT __declspec(dllimport)
#endif  // defined(HEADLESS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(HEADLESS_IMPLEMENTATION)
#define HEADLESS_EXPORT __attribute__((visibility("default")))
#else
#define HEADLESS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define HEADLESS_EXPORT
#endif

#endif  // HEADLESS_PUBLIC_HEADLESS_EXPORT_H_
