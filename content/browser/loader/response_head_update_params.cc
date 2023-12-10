// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/response_head_update_params.h"

namespace content {

ResponseHeadUpdateParams::ResponseHeadUpdateParams() = default;
ResponseHeadUpdateParams::~ResponseHeadUpdateParams() = default;

ResponseHeadUpdateParams::ResponseHeadUpdateParams(ResponseHeadUpdateParams&&) =
    default;
ResponseHeadUpdateParams& ResponseHeadUpdateParams::operator=(
    ResponseHeadUpdateParams&&) = default;

}  // namespace content
