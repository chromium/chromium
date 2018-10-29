// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_base.h"
#include "third_party/blink/public/web/web_element.h"

#include "url/gurl.h"

namespace blink {
class WebElement;
class WebFrame;
}  // namespace blink

namespace content {
struct WebPluginInfo;
}  // namespace content

namespace extensions {

// The frame-based implementation of MimeHandlerViewFrameContainer. This class
// performs tasks such as requesting resource, providing postMessage API, etc.
// for an embedded MimeHandlerView extension in a cross-origin frame.
class MimeHandlerViewFrameContainer : public MimeHandlerViewContainerBase {
 public:
  static bool Create(const blink::WebElement& plugin_element,
                     const GURL& resource_url,
                     const std::string& mime_type,
                     const content::WebPluginInfo& plugin_info,
                     int32_t element_instance_id);

 private:
  MimeHandlerViewFrameContainer(const blink::WebElement& plugin_element,
                                const GURL& resource_url,
                                const std::string& mime_type,
                                const content::WebPluginInfo& plugin_info,
                                int32_t element_instance_id);
  ~MimeHandlerViewFrameContainer() override;

  // MimeHandlerViewContainerBase overrides.
  void CreateMimeHandlerViewGuestIfNecessary() final;
  void OnRetryCreatingMimeHandlerViewGuest(int32_t element_instance_id) final;
  void OnDestroyFrameContainer(int32_t element_instance_id) final;
  blink::WebRemoteFrame* GetGuestProxyFrame() const final;
  int32_t GetInstanceId() const final;
  gfx::Size GetElementSize() const final;

  blink::WebFrame* GetContentFrame() const;

  // mime_handler::BeforeUnloadControl implementation.
  void SetShowBeforeUnloadDialog(
      bool show_dialog,
      SetShowBeforeUnloadDialogCallback callback) override;

  // Returns true if the container is considered as "embedded". A non-embedded
  // MimeHandlerViewFrameContainer is the one which is created as a result of
  // navigating a frame (either <iframe> or top-level) to a corresponding
  // MimeHandlerView mimetype. For such containers there is no need to request
  // the resource immediately.
  bool IsEmbedded() const;

  void OnMessageReceived(const IPC::Message& message);

  blink::WebElement plugin_element_;
  const int32_t element_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewFrameContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
