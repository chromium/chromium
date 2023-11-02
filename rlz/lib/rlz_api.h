// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_RLZ_API_H_
#define RLZ_LIB_RLZ_API_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#define RLZ_LIB_API __cdecl
#else
#define RLZ_LIB_API
#endif

#endif  // RLZ_LIB_RLZ_API_H_
