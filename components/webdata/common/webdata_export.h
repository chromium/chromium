// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEBDATA_EXPORT_H_
#define COMPONENTS_WEBDATA_COMMON_WEBDATA_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBDATA_IMPLEMENTATION)
#define WEBDATA_EXPORT __declspec(dllexport)
#else
#define WEBDATA_EXPORT __declspec(dllimport)
#endif  // defined(WEBDATA_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WEBDATA_IMPLEMENTATION)
#define WEBDATA_EXPORT __attribute__((visibility("default")))
#else
#define WEBDATA_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WEBDATA_EXPORT
#endif

#endif  // COMPONENTS_WEBDATA_COMMON_WEBDATA_EXPORT_H_
