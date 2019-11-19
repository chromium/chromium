// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_NS_VIEW_BRIDGE_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_NS_VIEW_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "content/common/content_export.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

@class WebContentsViewCocoa;

namespace content {
class WebContentsViewMac;
}  // namespace content

namespace remote_cocoa {

// A wrapper around a WebContentsViewCocoa, to be accessed via the mojo
// interface WebContentsNSViewBridge.
class CONTENT_EXPORT WebContentsNSViewBridge : public mojom::WebContentsNSView {
 public:
  // Create a bridge that will access its client in another process via a mojo
  // interface.
  WebContentsNSViewBridge(
      uint64_t view_id,
      mojo::PendingAssociatedRemote<mojom::WebContentsNSViewHost> client);
  // Create a bridge that will access its client directly in-process.
  // TODO(ccameron): Change this to expose only the mojom::WebContentsNSView
  // when all communication is through mojo.
  WebContentsNSViewBridge(uint64_t view_id,
                          content::WebContentsViewMac* web_contents_view);
  ~WebContentsNSViewBridge() override;

  WebContentsViewCocoa* GetNSView() const { return ns_view_.get(); }

  // mojom::WebContentsNSViewBridge:
  void SetParentNSView(uint64_t parent_ns_view_id) override;
  void ResetParentNSView() override;
  void SetBounds(const gfx::Rect& bounds_in_window) override;
  void SetVisible(bool visible) override;
  void MakeFirstResponder() override;
  void TakeFocus(bool reverse) override;
  void StartDrag(const content::DropData& drop_data,
                 uint32_t operation_mask,
                 const gfx::ImageSkia& image,
                 const gfx::Vector2d& image_offset) override;

 private:
  base::scoped_nsobject<WebContentsViewCocoa> ns_view_;
  mojo::AssociatedRemote<mojom::WebContentsNSViewHost> host_;

  std::unique_ptr<ScopedNSViewIdMapping> view_id_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsNSViewBridge);
};

}  // namespace content

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_NS_VIEW_BRIDGE_H_
