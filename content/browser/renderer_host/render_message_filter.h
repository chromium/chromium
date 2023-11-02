// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
class RenderWidgetHelper;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class RenderMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::RenderMessageFilter> {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      BrowserContext* browser_context,
                      RenderWidgetHelper* render_widget_helper);

  RenderMessageFilter(const RenderMessageFilter&) = delete;
  RenderMessageFilter& operator=(const RenderMessageFilter&) = delete;

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  int render_process_id() const { return render_process_id_; }

 protected:
  ~RenderMessageFilter() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderMessageFilter>;

  // mojom::RenderMessageFilter:
  void GenerateFrameRoutingID(GenerateFrameRoutingIDCallback callback) override;
  void HasGpuProcess(HasGpuProcessCallback callback) override;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  int render_process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
