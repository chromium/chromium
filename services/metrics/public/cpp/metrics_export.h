// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_METRICS_EXPORT_H_
#define SERVICES_METRICS_PUBLIC_CPP_METRICS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(METRICS_IMPLEMENTATION)
#define METRICS_EXPORT __declspec(dllexport)
#else
#define METRICS_EXPORT __declspec(dllimport)
#endif  // defined(METRICS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(METRICS_IMPLEMENTATION)
#define METRICS_EXPORT __attribute__((visibility("default")))
#else
#define METRICS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define METRICS_EXPORT
#endif

#endif  // SERVICES_METRICS_PUBLIC_CPP_METRICS_EXPORT_H_
