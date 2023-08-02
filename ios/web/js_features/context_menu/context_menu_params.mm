// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/context_menu_params.h"

namespace web {

ContextMenuParams::ContextMenuParams()
    : is_main_frame(true),
      tag_name(nil),
      referrer_policy(ReferrerPolicyDefault),
      location(CGPointZero),
      text_offset(0),
      surrounding_text_offset(0) {}

ContextMenuParams::ContextMenuParams(const ContextMenuParams& other) = default;

ContextMenuParams& ContextMenuParams::operator=(
    const ContextMenuParams& other) = default;

ContextMenuParams::ContextMenuParams(ContextMenuParams&& other) = default;

ContextMenuParams& ContextMenuParams::operator=(ContextMenuParams&& other) =
    default;

ContextMenuParams::~ContextMenuParams() {}

}  // namespace web
