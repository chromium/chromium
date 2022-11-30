// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/context_menu_params.h"

namespace content {

ContextMenuParams::ContextMenuParams() = default;
ContextMenuParams::ContextMenuParams(const ContextMenuParams& other) = default;
ContextMenuParams::~ContextMenuParams() = default;

ContextMenuParams::ContextMenuParams(
    const blink::UntrustworthyContextMenuParams& other)
    : UntrustworthyContextMenuParams(other) {}

}  // namespace content
