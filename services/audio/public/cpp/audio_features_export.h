// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_EXPORT_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(AUDIO_FEATURES_IMPLEMENTATION)
#define AUDIO_FEATURES_EXPORT __declspec(dllexport)
#else
#define AUDIO_FEATURES_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(AUDIO_FEATURES_IMPLEMENTATION)
#define AUDIO_FEATURES_EXPORT __attribute__((visibility("default")))
#else
#define AUDIO_FEATURES_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define AUDIO_FEATURES_EXPORT

#endif

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_EXPORT_H_
