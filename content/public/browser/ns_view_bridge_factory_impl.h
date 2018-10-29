// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_IMPL_H_
#define CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_IMPL_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/ns_view_bridge_factory.mojom.h"
#include "content/public/common/web_contents_ns_view_bridge.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {

// The factory that creates content NSView (RenderWidgetHostView and, if
// necessary, WebContentsView) instances. This is a singleton object that is
// to be instantiated in the app shim process.
class CONTENT_EXPORT NSViewBridgeFactoryImpl
    : public mojom::NSViewBridgeFactory {
 public:
  // Get the singleton instance of this factory for this app shim process.
  static NSViewBridgeFactoryImpl* Get();
  void BindRequest(mojom::NSViewBridgeFactoryAssociatedRequest request);

  // mojom::NSViewBridgeFactory:
  void CreateRenderWidgetHostNSViewBridge(
      mojom::StubInterfaceAssociatedPtrInfo client,
      mojom::StubInterfaceAssociatedRequest bridge_request) override;
  void CreateWebContentsNSViewBridge(
      uint64_t view_id,
      mojom::WebContentsNSViewClientAssociatedPtrInfo client,
      mojom::WebContentsNSViewBridgeAssociatedRequest bridge_request) override;

 private:
  friend class base::NoDestructor<NSViewBridgeFactoryImpl>;
  NSViewBridgeFactoryImpl();
  ~NSViewBridgeFactoryImpl() override;

  mojo::AssociatedBinding<mojom::NSViewBridgeFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(NSViewBridgeFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NS_VIEW_BRIDGE_FACTORY_IMPL_H_
