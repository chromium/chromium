// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_SERVICE_EXPORT_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_SERVICE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GPU_COMMAND_BUFFER_SERVICE_IMPLEMENTATION)
#define GPU_COMMAND_BUFFER_SERVICE_EXPORT __declspec(dllexport)
#else
#define GPU_COMMAND_BUFFER_SERVICE_EXPORT __declspec(dllimport)
#endif  // defined(GPU_IMPLEMENTATION)

#else  // defined(WIN32)
#define GPU_COMMAND_BUFFER_SERVICE_EXPORT __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD)
#define GPU_COMMAND_BUFFER_SERVICE_EXPORT
#endif

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_SERVICE_EXPORT_H_
