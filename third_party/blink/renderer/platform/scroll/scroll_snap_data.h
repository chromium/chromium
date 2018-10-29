// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_SCROLL_SNAP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_SCROLL_SNAP_DATA_H_

#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_fling_controller.h"
#include "cc/input/snap_selection_strategy.h"

// This file defines classes and structs used in SnapCoordinator.h

namespace blink {

using SnapAxis = cc::SnapAxis;
using SearchAxis = cc::SearchAxis;
using SnapStrictness = cc::SnapStrictness;
using SnapAlignment = cc::SnapAlignment;
using SnapSelectionStrategy = cc::SnapSelectionStrategy;
using ScrollSnapType = cc::ScrollSnapType;
using ScrollSnapAlign = cc::ScrollSnapAlign;
using SnapAreaData = cc::SnapAreaData;
using SnapContainerData = cc::SnapContainerData;
using SnapFlingController = cc::SnapFlingController;
using SnapFlingClient = cc::SnapFlingClient;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCROLL_SCROLL_SNAP_DATA_H_
