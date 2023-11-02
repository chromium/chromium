// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CRYPTO_EXPORT_H_
#define CRYPTO_CRYPTO_EXPORT_H_

// Defines CRYPTO_EXPORT so that functionality implemented by the crypto module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CRYPTO_IMPLEMENTATION)
#define CRYPTO_EXPORT __declspec(dllexport)
#else
#define CRYPTO_EXPORT __declspec(dllimport)
#endif  // defined(CRYPTO_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CRYPTO_IMPLEMENTATION)
#define CRYPTO_EXPORT __attribute__((visibility("default")))
#else
#define CRYPTO_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CRYPTO_EXPORT
#endif

#endif  // CRYPTO_CRYPTO_EXPORT_H_
