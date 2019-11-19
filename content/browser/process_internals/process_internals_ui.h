// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_
#define CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_

#include <memory>
#include <string>
#include <utility>

#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// WebUI which handles serving the chrome://process-internals page.
// TODO(nasko): Change the inheritance of this class to be from
// MojoWebUIController, so the registry_ can be removed and properly
// inherited from common base class for Mojo WebUIs.
class ProcessInternalsUI : public WebUIController, public WebContentsObserver {
 public:
  explicit ProcessInternalsUI(WebUI* web_ui);
  ~ProcessInternalsUI() override;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindProcessInternalsHandler(
      mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver,
      RenderFrameHost* render_frame_host);

 private:
  std::unique_ptr<::mojom::ProcessInternalsHandler> ui_handler_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_UI_H_
