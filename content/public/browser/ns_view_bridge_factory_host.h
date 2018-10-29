// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_HOST_H_
#define CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_HOST_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/ns_view_bridge_factory.mojom.h"

namespace content {

// The host interface to the factory that will instantiate content NSViews
// (RenderWidgetHostView and WebContentsView) in an app shim process. Each
// app shim will create an instance of this class for to create windows in
// its app.
class CONTENT_EXPORT NSViewBridgeFactoryHost {
 public:
  NSViewBridgeFactoryHost(mojom::NSViewBridgeFactoryAssociatedRequest* request,
                          uint64_t host_id);
  ~NSViewBridgeFactoryHost();

  // The host id that refers to creating NSViews in the local process, and
  // and accessing directly via pointers (instead of through mojo pipes).
  static const uint64_t kLocalDirectHostId;

  // Look up a NSViewBridgeFactoryHost from the host id. This host id can
  // be used (e.g, by views::NativeViewHost) to associate different Cocoa
  // factories that refer to the same app shim process.
  static NSViewBridgeFactoryHost* GetFromHostId(uint64_t host_id);

  mojom::NSViewBridgeFactory* GetFactory();

 private:
  const uint64_t host_id_;
  mojom::NSViewBridgeFactoryAssociatedPtr factory_;

  DISALLOW_COPY_AND_ASSIGN(NSViewBridgeFactoryHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_HOST_H_
