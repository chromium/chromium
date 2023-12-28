// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_PROXY_EXPORT_H_
#define PPAPI_PROXY_PPAPI_PROXY_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PPAPI_PROXY_IMPLEMENTATION)
#define PPAPI_PROXY_EXPORT __declspec(dllexport)
#else
#define PPAPI_PROXY_EXPORT __declspec(dllimport)
#endif  // defined(PPAPI_PROXY_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PPAPI_PROXY_IMPLEMENTATION)
#define PPAPI_PROXY_EXPORT __attribute__((visibility("default")))
#else
#define PPAPI_PROXY_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PPAPI_PROXY_EXPORT
#endif

#endif  // PPAPI_PROXY_PPAPI_PROXY_EXPORT_H_
