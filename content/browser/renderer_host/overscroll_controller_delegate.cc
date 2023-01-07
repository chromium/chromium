// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller_delegate.h"

namespace content {

OverscrollControllerDelegate::OverscrollControllerDelegate() = default;
OverscrollControllerDelegate::~OverscrollControllerDelegate() = default;

base::WeakPtr<OverscrollControllerDelegate>
OverscrollControllerDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
