// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_OBSERVER_H_
#define SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "net/http/http_response_headers.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

class COMPONENT_EXPORT(CONTENT_SERVICE_CPP) NavigableContentsObserver
    : public base::CheckedObserver {
 public:
  virtual void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const net::HttpResponseHeaders* response_headers) {}
  virtual void DidStopLoading() {}
  virtual void DidAutoResizeView(const gfx::Size& new_size) {}
  virtual void DidSuppressNavigation(const GURL& url,
                                     WindowOpenDisposition disposition,
                                     bool from_user_gesture) {}
  virtual void UpdateCanGoBack(bool can_go_back) {}
  virtual void FocusedNodeChanged(bool is_editable_node,
                                  const gfx::Rect& node_bounds_in_screen) {}
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_OBSERVER_H_
