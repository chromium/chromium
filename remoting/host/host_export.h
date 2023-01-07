// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXPORT_H_
#define REMOTING_HOST_HOST_EXPORT_H_

#if defined(WIN32)

#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __declspec(dllexport)
#else
#define HOST_EXPORT __declspec(dllimport)
#endif  // defined(HOST_IMPLEMENTATION)

#else  // !defined(WIN32)
#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __attribute__((visibility("default")))
#else
#define HOST_EXPORT
#endif  // defined(HOST_IMPLEMENTATION)
#endif  // !defined(WIN32)

#endif  // REMOTING_HOST_HOST_EXPORT_H_
