// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_NS_VIEW_BRIDGE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_NS_VIEW_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/web_contents_ns_view_bridge.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/base/cocoa/ns_view_ids.h"

namespace content {

// A C++ wrapper around a WebContentsView's NSView in a non-browser process.
class CONTENT_EXPORT WebContentsNSViewBridge
    : public mojom::WebContentsNSViewBridge {
 public:
  // Create a bridge that will access its client in another process via a mojo
  // interface. This object will be deleted when |bridge_request|'s connection
  // closes.
  WebContentsNSViewBridge(
      uint64_t view_id,
      mojom::WebContentsNSViewClientAssociatedPtr client,
      mojom::WebContentsNSViewBridgeAssociatedRequest bridge_request);

  // mojom::WebContentsNSViewBridge:
  void SetParentViewsNSView(uint64_t parent_ns_view_id) override;
  void Show(const gfx::Rect& bounds_in_window) override;
  void Hide() override;
  void MakeFirstResponder() override;

 private:
  ~WebContentsNSViewBridge() override;
  void OnConnectionError();

  base::scoped_nsobject<NSView> cocoa_view_;
  mojom::WebContentsNSViewClientAssociatedPtr client_;
  mojo::AssociatedBinding<mojom::WebContentsNSViewBridge> binding_;

  std::unique_ptr<ui::ScopedNSViewIdMapping> view_id_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsNSViewBridge);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_NS_VIEW_BRIDGE_H_
