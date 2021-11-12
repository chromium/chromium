// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_

#include <string>

#include "content/public/renderer/render_frame_observer.h"

namespace extensions {

// Renderer-side implementation for chrome.automation API (for the few pieces
// which aren't built in to the existing accessibility system).
class AutomationApiHelper : public content::RenderFrameObserver {
 public:
  explicit AutomationApiHelper(content::RenderFrame* render_frame);

  AutomationApiHelper(const AutomationApiHelper&) = delete;
  AutomationApiHelper& operator=(const AutomationApiHelper&) = delete;

  ~AutomationApiHelper() override;

 private:
  // content::RenderFrameObserver:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnQuerySelector(int acc_obj_id,
                       int request_id,
                       const std::u16string& selector);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
