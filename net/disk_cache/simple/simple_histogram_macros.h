// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_MACROS_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_MACROS_H_

#include "base/metrics/histogram_macros.h"
#include "net/base/cache_type.h"

// This file contains macros used to report histograms. The main issue is that
// we want to have separate histograms for each type of cache (app, http, and
// media), while making it easy to report histograms and have all names
// precomputed.

#define SIMPLE_CACHE_THUNK(uma_prefix, uma_type, args) \
  uma_prefix##_HISTOGRAM_##uma_type args

// TODO(pasko): add histograms for shader cache as soon as it becomes possible
// for a user to get shader cache with the |SimpleBackendImpl| without altering
// any flags.
#define SIMPLE_CACHE_HISTO(uma_prefix, uma_type, uma_name, cache_type, ...) \
  do {                                                                      \
    switch (cache_type) {                                                   \
      case net::DISK_CACHE:                                                 \
        SIMPLE_CACHE_THUNK(uma_prefix, uma_type,                            \
                           ("SimpleCache.Http." uma_name, ##__VA_ARGS__));  \
        break;                                                              \
      case net::APP_CACHE:                                                  \
        SIMPLE_CACHE_THUNK(uma_prefix, uma_type,                            \
                           ("SimpleCache.App." uma_name, ##__VA_ARGS__));   \
        break;                                                              \
      case net::GENERATED_BYTE_CODE_CACHE:                                  \
        SIMPLE_CACHE_THUNK(uma_prefix, uma_type,                            \
                           ("SimpleCache.Code." uma_name, ##__VA_ARGS__));  \
        break;                                                              \
      case net::GENERATED_NATIVE_CODE_CACHE:                                \
      case net::GENERATED_WEBUI_BYTE_CODE_CACHE:                            \
      case net::SHADER_CACHE:                                               \
        break;                                                              \
      default:                                                              \
        NOTREACHED_IN_MIGRATION();                                          \
        break;                                                              \
    }                                                                       \
  } while (0)

#define SIMPLE_CACHE_UMA(uma_type, uma_name, cache_type, ...) \
  SIMPLE_CACHE_HISTO(UMA, uma_type, uma_name, cache_type, ##__VA_ARGS__)

#define SIMPLE_CACHE_LOCAL(uma_type, uma_name, cache_type, ...) \
  SIMPLE_CACHE_HISTO(LOCAL, uma_type, uma_name, cache_type, ##__VA_ARGS__)

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_MACROS_H_
