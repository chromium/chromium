// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_DEVICE_BASE_EXPORT_H_
#define DEVICE_BASE_DEVICE_BASE_EXPORT_H_

#if defined(COMPONENT_BUILD) && defined(WIN32)

#if defined(DEVICE_BASE_IMPLEMENTATION)
#define DEVICE_BASE_EXPORT __declspec(dllexport)
#else
#define DEVICE_BASE_EXPORT __declspec(dllimport)
#endif

#elif defined(COMPONENT_BUILD) && !defined(WIN32)

#if defined(DEVICE_BASE_IMPLEMENTATION)
#define DEVICE_BASE_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_BASE_EXPORT
#endif

#else
#define DEVICE_BASE_EXPORT
#endif

#endif  // DEVICE_BASE_DEVICE_BASE_EXPORT_H_
