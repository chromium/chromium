// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_SHARED_EXPORT_H_
#define PPAPI_SHARED_IMPL_PPAPI_SHARED_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PPAPI_SHARED_IMPLEMENTATION)
#define PPAPI_SHARED_EXPORT __declspec(dllexport)
#else
#define PPAPI_SHARED_EXPORT __declspec(dllimport)
#endif  // defined(PPAPI_SHARED_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PPAPI_SHARED_IMPLEMENTATION)
#define PPAPI_SHARED_EXPORT __attribute__((visibility("default")))
#else
#define PPAPI_SHARED_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PPAPI_SHARED_EXPORT
#endif

#endif  // PPAPI_SHARED_IMPL_PPAPI_SHARED_EXPORT_H_
