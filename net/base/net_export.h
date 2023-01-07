// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_EXPORT_H_
#define NET_BASE_NET_EXPORT_H_

// Defines NET_EXPORT so that functionality implemented by the net module can
// be exported to consumers, and NET_EXPORT_PRIVATE that allows unit tests to
// access features not intended to be used directly by real consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(NET_IMPLEMENTATION)
#define NET_EXPORT __declspec(dllexport)
#define NET_EXPORT_PRIVATE __declspec(dllexport)
#else
#define NET_EXPORT __declspec(dllimport)
#define NET_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(NET_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(NET_IMPLEMENTATION)
#define NET_EXPORT __attribute__((visibility("default")))
#define NET_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define NET_EXPORT
#define NET_EXPORT_PRIVATE
#endif
#endif

#else  /// defined(COMPONENT_BUILD)
#define NET_EXPORT
#define NET_EXPORT_PRIVATE
#endif

#endif  // NET_BASE_NET_EXPORT_H_
