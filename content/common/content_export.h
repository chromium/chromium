// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_EXPORT_H_
#define CONTENT_COMMON_CONTENT_EXPORT_H_

#if defined(COMPONENT_BUILD) && !defined(COMPILE_CONTENT_STATICALLY)
#if defined(WIN32)

#if defined(CONTENT_IMPLEMENTATION)
#define CONTENT_EXPORT __declspec(dllexport)
#else
#define CONTENT_EXPORT __declspec(dllimport)
#endif  // defined(CONTENT_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(CONTENT_IMPLEMENTATION)
#define CONTENT_EXPORT __attribute__((visibility("default")))
#else
#define CONTENT_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define CONTENT_EXPORT
#endif

#endif  // CONTENT_COMMON_CONTENT_EXPORT_H_
