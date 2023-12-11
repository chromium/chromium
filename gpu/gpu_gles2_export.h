// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GPU_GLES2_EXPORT_H_
#define GPU_GPU_GLES2_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GPU_GLES2_IMPLEMENTATION)
#define GPU_GLES2_EXPORT __declspec(dllexport)
#else
#define GPU_GLES2_EXPORT __declspec(dllimport)
#endif  // defined(GPU_GLES2_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GPU_GLES2_IMPLEMENTATION)
#define GPU_GLES2_EXPORT __attribute__((visibility("default")))
#else
#define GPU_GLES2_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GPU_GLES2_EXPORT
#endif

#endif  // GPU_GPU_GLES2_EXPORT_H_
