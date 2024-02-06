// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_INTERNAL_HEADERS_H_
#define REMOTING_BASE_INTERNAL_HEADERS_H_

#include "remoting/base/buildflags.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/base/api_keys.h"
#include "remoting/internal/proto/helpers.h"
#else
#include "remoting/base/api_key_stubs.h"    // nogncheck
#include "remoting/proto/internal_stubs.h"  // nogncheck
#endif

#endif  // REMOTING_BASE_INTERNAL_HEADERS_H_
