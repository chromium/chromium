// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_EXPORT_H_
#define DEVICE_VR_VR_EXPORT_H_

#if defined(COMPONENT_BUILD) && defined(WIN32)

#if defined(DEVICE_VR_IMPLEMENTATION)
#define DEVICE_VR_EXPORT __declspec(dllexport)
#else
#define DEVICE_VR_EXPORT __declspec(dllimport)
#endif

#elif defined(COMPONENT_BUILD) && !defined(WIN32)

#if defined(DEVICE_VR_IMPLEMENTATION)
#define DEVICE_VR_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_VR_EXPORT
#endif

#else
#define DEVICE_VR_EXPORT
#endif

#endif  // DEVICE_VR_VR_EXPORT_H_
