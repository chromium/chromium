// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#import <UIKit/UIKit.h>

#include "base/notimplemented.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace content {

void WebContentsImpl::Resize(const gfx::Rect& new_bounds) {
  NOTIMPLEMENTED();
}

gfx::Size WebContentsImpl::GetSize() {
  UIView* view = GetNativeView().Get();
  DCHECK(view);
  CGRect frame = view.frame;
  return gfx::Size(frame.size.width, frame.size.height);
}

}  // namespace content
