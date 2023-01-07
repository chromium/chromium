// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_ITERATOR_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_ITERATOR_H_

namespace content {

class RenderWidgetHost;

// RenderWidgetHostIterator is used to safely iterate over a list of
// RenderWidgetHosts.
class RenderWidgetHostIterator {
 public:
  virtual ~RenderWidgetHostIterator() {}

  // Returns the next RenderWidgetHost in the list. Returns nullptr if none is
  // available.
  virtual RenderWidgetHost* GetNextHost() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_ITERATOR_H_
