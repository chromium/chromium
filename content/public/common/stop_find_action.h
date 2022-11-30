// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_
#define CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-shared.h"

namespace content {

// DEPRECATED - For future usage, use blink::mojom::StopFindAction directly.
// The user has completed a find-in-page; this type defines what actions the
// renderer should take next.
enum StopFindAction {
  STOP_FIND_ACTION_CLEAR_SELECTION = static_cast<int>(
      blink::mojom::StopFindAction::kStopFindActionClearSelection),
  STOP_FIND_ACTION_KEEP_SELECTION = static_cast<int>(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection),
  STOP_FIND_ACTION_ACTIVATE_SELECTION = static_cast<int>(
      blink::mojom::StopFindAction::kStopFindActionActivateSelection),
  STOP_FIND_ACTION_LAST =
      static_cast<int>(blink::mojom::StopFindAction::kMaxValue)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_
