// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CAPTURE_EXPORT_H_
#define MEDIA_CAPTURE_CAPTURE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CAPTURE_IMPLEMENTATION)
#define CAPTURE_EXPORT __declspec(dllexport)
#else
#define CAPTURE_EXPORT __declspec(dllimport)
#endif  // defined(CAPTURE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CAPTURE_IMPLEMENTATION)
#define CAPTURE_EXPORT __attribute__((visibility("default")))
#else
#define CAPTURE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CAPTURE_EXPORT
#endif

#endif  // MEDIA_CAPTURE_CAPTURE_EXPORT_H_
