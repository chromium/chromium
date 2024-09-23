// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SWITCHES_EXPORT_H_
#define SKIA_EXT_SWITCHES_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SKIA_SWITCHES_IMPLEMENTATION)
#define SKIA_SWITCHES_EXPORT __declspec(dllexport)
#else
#define SKIA_SWITCHES_EXPORT __declspec(dllimport)
#endif  // defined(SKIA_SWITCHES_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SKIA_SWITCHES_IMPLEMENTATION)
#define SKIA_SWITCHES_EXPORT __attribute__((visibility("default")))
#else
#define SKIA_SWITCHES_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SKIA_SWITCHES_EXPORT
#endif

#endif  // SKIA_EXT_SWITCHES_EXPORT_H_
