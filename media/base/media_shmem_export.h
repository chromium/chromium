// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_SHMEM_EXPORT_H_
#define MEDIA_BASE_MEDIA_SHMEM_EXPORT_H_

// Define MEDIA_SHMEM_EXPORT so that functionality implemented by the
// shared_memory_support module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MEDIA_SHMEM_IMPLEMENTATION)
#define MEDIA_SHMEM_EXPORT __declspec(dllexport)
#else
#define MEDIA_SHMEM_EXPORT __declspec(dllimport)
#endif  // defined(MEDIA_SHMEM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MEDIA_SHMEM_IMPLEMENTATION)
#define MEDIA_SHMEM_EXPORT __attribute__((visibility("default")))
#else
#define MEDIA_SHMEM_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MEDIA_SHMEM_EXPORT
#endif

#endif  // MEDIA_BASE_MEDIA_SHMEM_EXPORT_H_
