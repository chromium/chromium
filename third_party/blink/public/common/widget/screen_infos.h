// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFOS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFOS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/widget/screen_info.h"

namespace blink {

// Information about a set of screens that are relevant to a particular widget.
// This includes an id for the screen currently showing the widget.
struct BLINK_COMMON_EXPORT ScreenInfos {
  ScreenInfos();
  explicit ScreenInfos(const ScreenInfo& screen_info);
  ScreenInfos(const ScreenInfos& other);
  ~ScreenInfos();
  ScreenInfos& operator=(const ScreenInfos& other);
  bool operator==(const ScreenInfos& other) const;
  bool operator!=(const ScreenInfos& other) const;

  // Helpers to access the current ScreenInfo element.
  ScreenInfo& mutable_current();
  const ScreenInfo& current() const;

  std::vector<ScreenInfo> screen_infos;
  // The display_id of the current ScreenInfo in `screen_infos`.
  int64_t current_display_id = ScreenInfo::kInvalidDisplayId;
};

}  // namespace blink

#endif
