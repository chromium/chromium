// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "content/browser/media/media_internals_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace content {

MediaInternalsMessageHandler::MediaInternalsMessageHandler()
    : proxy_(new MediaInternalsProxy()),
      page_load_complete_(false) {}

MediaInternalsMessageHandler::~MediaInternalsMessageHandler() {
  proxy_->Detach();
}

void MediaInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  proxy_->Attach(this);

  web_ui()->RegisterMessageCallback(
      "getEverything",
      base::BindRepeating(&MediaInternalsMessageHandler::OnGetEverything,
                          base::Unretained(this)));
}

void MediaInternalsMessageHandler::OnGetEverything(
    const base::ListValue* list) {
  page_load_complete_ = true;
  proxy_->GetEverything();
}

void MediaInternalsMessageHandler::OnUpdate(const base::string16& update) {
  // Don't try to execute JavaScript in a RenderView that no longer exists nor
  // if the chrome://media-internals page hasn't finished loading.
  RenderFrameHost* host = web_ui()->GetWebContents()->GetMainFrame();
  if (host && page_load_complete_)
    host->ExecuteJavaScript(update, base::NullCallback());
}

}  // namespace content
