// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_HANDLER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class MediaInternalsProxy;

// This class handles messages to and from MediaInternalsUI.
// It does all its work on the IO thread through the proxy below.
class MediaInternalsMessageHandler : public WebUIMessageHandler {
 public:
  MediaInternalsMessageHandler();

  MediaInternalsMessageHandler(const MediaInternalsMessageHandler&) = delete;
  MediaInternalsMessageHandler& operator=(const MediaInternalsMessageHandler&) =
      delete;

  ~MediaInternalsMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Javascript message handlers.
  void OnGetEverything(const base::Value::List& list);

  // MediaInternals message handlers.
  void OnUpdate(const std::u16string& update);

 private:
  scoped_refptr<MediaInternalsProxy> proxy_;

  // Reflects whether the chrome://media-internals HTML+JS has finished loading.
  // If not, it's not safe to send JavaScript calls targeting the page yet.
  bool page_load_complete_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_HANDLER_H_
