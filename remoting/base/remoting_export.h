// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_REMOTING_EXPORT_H_
#define REMOTING_BASE_REMOTING_EXPORT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// On Linux, we need to export this symbol so it can be used by binaries that
// link against remoting_core.so.
#if defined(WIN32)
#if defined(HOST_IMPLEMENTATION)
#define REMOTING_EXPORT __declspec(dllexport)
#else
#define REMOTING_EXPORT __declspec(dllimport)
#endif  // defined(HOST_IMPLEMENTATION)
#else   // !defined(WIN32)
#define REMOTING_EXPORT __attribute__((visibility("default")))
#endif  // !defined(WIN32)
#else   // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
#define REMOTING_EXPORT
#endif

#endif  // REMOTING_BASE_REMOTING_EXPORT_H_
