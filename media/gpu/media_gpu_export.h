// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MEDIA_GPU_EXPORT_H_
#define MEDIA_GPU_MEDIA_GPU_EXPORT_H_

// Define MEDIA_GPU_EXPORT so that functionality implemented by the Media GPU
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MEDIA_GPU_IMPLEMENTATION)
#define MEDIA_GPU_EXPORT __declspec(dllexport)
#else
#define MEDIA_GPU_EXPORT __declspec(dllimport)
#endif  // defined(MEDIA_GPU_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MEDIA_GPU_IMPLEMENTATION)
#define MEDIA_GPU_EXPORT __attribute__((visibility("default")))
#else
#define MEDIA_GPU_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MEDIA_GPU_EXPORT
#endif

#endif  // MEDIA_GPU_MEDIA_GPU_EXPORT_H_
