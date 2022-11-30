// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_EXPORT_H_
#define SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SERVICES_DEVICE_FINGERPRINT_IMPLEMENTATION)
#define SERVICES_DEVICE_FINGERPRINT_EXPORT __declspec(dllexport)
#else
#define SERVICES_DEVICE_FINGERPRINT_EXPORT __declspec(dllimport)
#endif  // defined(SERVICES_DEVICE_FINGERPRINT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SERVICES_DEVICE_FINGERPRINT_IMPLEMENTATION)
#define SERVICES_DEVICE_FINGERPRINT_EXPORT \
  __attribute__((visibility("default")))
#else
#define SERVICES_DEVICE_FINGERPRINT_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SERVICES_DEVICE_FINGERPRINT_EXPORT
#endif

#endif  // SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_EXPORT_H_
