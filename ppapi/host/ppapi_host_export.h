// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_PPAPI_HOST_EXPORT_H_
#define PPAPI_HOST_PPAPI_HOST_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PPAPI_HOST_IMPLEMENTATION)
#define PPAPI_HOST_EXPORT __declspec(dllexport)
#else
#define PPAPI_HOST_EXPORT __declspec(dllimport)
#endif  // defined(PPAPI_HOST_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PPAPI_HOST_IMPLEMENTATION)
#define PPAPI_HOST_EXPORT __attribute__((visibility("default")))
#else
#define PPAPI_HOST_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PPAPI_HOST_EXPORT
#endif

#endif  // PPAPI_HOST_PPAPI_HOST_EXPORT_H_
