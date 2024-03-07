// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_COOPERATIVE_SCHEDULING_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_COOPERATIVE_SCHEDULING_HELPERS_H_

#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#if BUILDFLAG(BLINK_BINDINGS_COOPERATIVE_SCHEDULING_ENABLED)
#define BINDINGS_COOPERATIVE_SCHEDULING_SAFEPOINT()                   \
  do {                                                                \
    scheduler::CooperativeSchedulingManager::Instance()->Safepoint(); \
  } while (false)
#else
#define BINDINGS_COOPERATIVE_SCHEDULING_SAFEPOINT() \
  do {                                              \
  } while (false)
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_COOPERATIVE_SCHEDULING_HELPERS_H_
