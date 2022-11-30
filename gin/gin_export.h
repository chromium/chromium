// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_EXPORT_H_
#define GIN_GIN_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GIN_IMPLEMENTATION)
#define GIN_EXPORT __declspec(dllexport)
#else
#define GIN_EXPORT __declspec(dllimport)
#endif  // defined(GIN_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GIN_IMPLEMENTATION)
#define GIN_EXPORT __attribute__((visibility("default")))
#else
#define GIN_EXPORT
#endif  // defined(GIN_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define GIN_EXPORT
#endif

#endif  // GIN_GIN_EXPORT_H_
