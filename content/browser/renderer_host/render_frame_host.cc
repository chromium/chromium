// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_frame_host.h"

// render_frame_host.h is a widely included header and its size has significant
// impact on build time. Try not to raise this limit unless necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 650000
#endif
