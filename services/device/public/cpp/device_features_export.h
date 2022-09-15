// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_EXPORT_H_
#define SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(DEVICE_FEATURES_IMPLEMENTATION)
#define DEVICE_FEATURES_EXPORT __declspec(dllexport)
#else
#define DEVICE_FEATURES_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(DEVICE_FEATURES_IMPLEMENTATION)
#define DEVICE_FEATURES_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_FEATURES_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define DEVICE_FEATURES_EXPORT

#endif

#endif  // SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_EXPORT_H_
