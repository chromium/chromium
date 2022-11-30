// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_WIFI_EXPORT_H_
#define COMPONENTS_WIFI_WIFI_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WIFI_IMPLEMENTATION)
#define WIFI_EXPORT __declspec(dllexport)
#else
#define WIFI_EXPORT __declspec(dllimport)
#endif  // defined(WIFI_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WIFI_IMPLEMENTATION)
#define WIFI_EXPORT __attribute__((visibility("default")))
#else
#define WIFI_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WIFI_EXPORT
#endif

#endif  // COMPONENTS_WIFI_WIFI_EXPORT_H_
