// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

//
// RenderWidgetDelegate
//
//  An interface to provide View-level (and/or Page-level) functionality to
//  the main frame's RenderWidget.
class CONTENT_EXPORT RenderWidgetDelegate {
 public:
  virtual ~RenderWidgetDelegate() = default;

  // Returns whether multiple windows are allowed for the widget. If true, then
  // Show() may be called more than once.
  virtual bool SupportsMultipleWindowsForWidget() = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
