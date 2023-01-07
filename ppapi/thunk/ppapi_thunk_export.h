// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPAPI_THUNK_EXPORT_H_
#define PPAPI_THUNK_PPAPI_THUNK_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PPAPI_THUNK_IMPLEMENTATION)
#define PPAPI_THUNK_EXPORT __declspec(dllexport)
#else
#define PPAPI_THUNK_EXPORT __declspec(dllimport)
#endif  // defined(PPAPI_THUNK_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PPAPI_THUNK_IMPLEMENTATION)
#define PPAPI_THUNK_EXPORT __attribute__((visibility("default")))
#else
#define PPAPI_THUNK_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PPAPI_THUNK_EXPORT
#endif

#endif  // PPAPI_THUNK_PPAPI_THUNK_EXPORT_H_
