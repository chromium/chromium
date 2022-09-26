// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comes from bionic/linker/linker_common_types.h
// Android uses RELA for aarch64 and x86_64. mips64 still uses REL.
#if defined(__aarch64__) || defined(__x86_64__)
// USE_RELA means relocations have explicit addends.
#define USE_RELA 1
#endif
