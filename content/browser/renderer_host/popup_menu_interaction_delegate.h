// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_INTERACTION_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_INTERACTION_DELEGATE_H_

namespace content {

class MenuInteractionDelegate {
 public:
  virtual void OnMenuItemSelected(int idx) = 0;
  virtual void OnMenuCanceled() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_INTERACTION_DELEGATE_H_
