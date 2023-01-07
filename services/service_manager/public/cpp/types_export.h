// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SERVICE_MANAGER_PUBLIC_CPP_TYPES_IMPL)
#define SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT __declspec(dllexport)
#else
#define SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT __declspec(dllimport)
#endif  // defined(SERVICE_MANAGER_PUBLIC_CPP_TYPES_IMPL)

#else  // defined(WIN32)
#if defined(SERVICE_MANAGER_PUBLIC_CPP_TYPES_IMPL)
#define SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT \
  __attribute__((visibility("default")))
#else
#define SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT
#endif  // defined(SERVICE_MANAGER_PUBLIC_CPP_TYPES_IMPL)
#endif

#else  // defined(COMPONENT_BUILD)
#define SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT
#endif

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT_H_
