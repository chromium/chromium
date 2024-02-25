// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_RASTER_EXPORT_H_
#define GPU_RASTER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(RASTER_IMPLEMENTATION)
#define RASTER_EXPORT __declspec(dllexport)
#else
#define RASTER_EXPORT __declspec(dllimport)
#endif  // defined(RASTER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(RASTER_IMPLEMENTATION)
#define RASTER_EXPORT __attribute__((visibility("default")))
#else
#define RASTER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define RASTER_EXPORT
#endif

#endif  // GPU_RASTER_EXPORT_H_
