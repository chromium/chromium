// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_FACTORY_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/widget.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class RenderProcessHost;
class RenderWidgetHostDelegate;
class RenderWidgetHostImpl;

// A factory for creating RenderWidgetHostImpls. There is a global factory
// function that can be installed for the purposes of testing to provide a
// specialized RenderWidgetHostImpl class.
class RenderWidgetHostFactory {
 public:
  // Creates a RenderWidgetHostImpl using the currently registered factory, or
  // the default one if no factory is registered. Ownership of the returned
  // pointer will be passed to the caller.
  static std::unique_ptr<RenderWidgetHostImpl> Create(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int32_t routing_id,
      mojo::PendingRemote<mojom::Widget> widget_interface,
      bool hidden);

  // Returns true if there is currently a globally-registered factory.
  static bool has_factory() { return !!factory_; }

 protected:
  RenderWidgetHostFactory() {}
  virtual ~RenderWidgetHostFactory() {}

  // You can derive from this class and specify an implementation for this
  // function to create a different kind of RenderWidgetHostImpl for testing.
  virtual std::unique_ptr<RenderWidgetHostImpl> CreateRenderWidgetHost(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int32_t routing_id,
      mojo::PendingRemote<mojom::Widget> widget_interface,
      bool hidden) = 0;

  // Registers your factory to be called when new RenderWidgetHostImpls are
  // created. We have only one global factory, so there must be no factory
  // registered before the call. This class does NOT take ownership of the
  // pointer.
  CONTENT_EXPORT static void RegisterFactory(RenderWidgetHostFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // the default RenderWidgetHostImpls will be created.
  CONTENT_EXPORT static void UnregisterFactory();

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default RenderWidgetHostImpls.
  CONTENT_EXPORT static RenderWidgetHostFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_FACTORY_H_
