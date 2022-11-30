// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_EXPORT_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(GAMEPAD_FEATURES_IMPLEMENTATION)
#define GAMEPAD_FEATURES_EXPORT __declspec(dllexport)
#else
#define GAMEPAD_FEATURES_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(GAMEPAD_FEATURES_IMPLEMENTATION)
#define GAMEPAD_FEATURES_EXPORT __attribute__((visibility("default")))
#else
#define GAMEPAD_FEATURES_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define GAMEPAD_FEATURES_EXPORT

#endif

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_EXPORT_H_