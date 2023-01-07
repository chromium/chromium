// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_EXPORT_H_
#define EXTENSIONS_COMMON_EXTENSIONS_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(EXTENSIONS_COMPONENT_IMPLEMENTATION)
#define EXTENSIONS_EXPORT __declspec(dllexport)
#else
#define EXTENSIONS_EXPORT __declspec(dllimport)
#endif  // defined(EXTENSIONS_COMPONENT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(EXTENSIONS_COMPONENT_IMPLEMENTATION)
#define EXTENSIONS_EXPORT __attribute__((visibility("default")))
#else
#define EXTENSIONS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define EXTENSIONS_EXPORT
#endif

#endif  // EXTENSIONS_COMMON_EXTENSIONS_EXPORT_H_
