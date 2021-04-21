// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_MOJO_EXPORT_H_
#define MEDIA_MOJO_SERVICES_MEDIA_MOJO_EXPORT_H_

// Define MEDIA_MOJO_EXPORT so that functionality implemented by the
// media/mojo module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MEDIA_MOJO_IMPLEMENTATION)
#define MEDIA_MOJO_EXPORT __declspec(dllexport)
#else
#define MEDIA_MOJO_EXPORT __declspec(dllimport)
#endif  // defined(MEDIA_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MEDIA_MOJO_IMPLEMENTATION)
#define MEDIA_MOJO_EXPORT __attribute__((visibility("default")))
#else
#define MEDIA_MOJO_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MEDIA_MOJO_EXPORT
#endif

#endif  // MEDIA_MOJO_SERVICES_MEDIA_MOJO_EXPORT_H_
