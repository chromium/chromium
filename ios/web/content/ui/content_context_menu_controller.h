// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_UI_CONTENT_CONTEXT_MENU_CONTROLLER_H_
#define IOS_WEB_CONTENT_UI_CONTENT_CONTEXT_MENU_CONTROLLER_H_

#import <memory>

#import "base/memory/ref_counted.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

class IOSWebContentsUIButtonHolder;

class ContentContextMenuController
    : public base::RefCountedThreadSafe<ContentContextMenuController> {
 public:
  ContentContextMenuController();

  ContentContextMenuController(const ContentContextMenuController&) = delete;
  ContentContextMenuController& operator=(const ContentContextMenuController&) =
      delete;

  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params);

 private:
  friend class base::RefCountedThreadSafe<ContentContextMenuController>;
  virtual ~ContentContextMenuController();

  // A hidden button used for displaying context menus.
  std::unique_ptr<IOSWebContentsUIButtonHolder> hidden_button_;
};

#endif  // IOS_WEB_CONTENT_UI_CONTENT_CONTEXT_MENU_CONTROLLER_H_
