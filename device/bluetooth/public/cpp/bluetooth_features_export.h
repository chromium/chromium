// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_EXPORT_H_
#define DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(BLUETOOTH_FEATURES_IMPLEMENTATION)
#define BLUETOOTH_FEATURES_EXPORT __declspec(dllexport)
#else
#define BLUETOOTH_FEATURES_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(BLUETOOTH_FEATURES_IMPLEMENTATION)
#define BLUETOOTH_FEATURES_EXPORT __attribute__((visibility("default")))
#else
#define BLUETOOTH_FEATURES_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define BLUETOOTH_FEATURES_EXPORT

#endif

#endif  // DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_FEATURES_EXPORT_H_
