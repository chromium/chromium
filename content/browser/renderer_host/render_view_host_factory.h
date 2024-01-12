// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_

#include <stdint.h>

#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/render_view_host_enums.h"
#include "content/common/content_export.h"

namespace content {
class FrameTree;
class RenderViewHost;
class RenderViewHostDelegate;
class RenderWidgetHostDelegate;
class SiteInstanceGroup;

// A factory for creating RenderViewHosts. There is a global factory function
// that can be installed for the purposes of testing to provide a specialized
// RenderViewHost class.
class RenderViewHostFactory {
 public:
  // Creates a RenderViewHost using the currently registered factory, or the
  // default one if no factory is registered. Ownership of the returned
  // pointer will be passed to the caller.
  static RenderViewHost* Create(
      FrameTree* frame_tree,
      SiteInstanceGroup* group,
      const StoragePartitionConfig& storage_partition_config,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int32_t main_frame_routing_id,
      bool renderer_initiated_creation,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case,
      std::optional<viz::FrameSinkId> frame_sink_id);

  RenderViewHostFactory(const RenderViewHostFactory&) = delete;
  RenderViewHostFactory& operator=(const RenderViewHostFactory&) = delete;

  // Returns true if there is currently a globally-registered factory.
  static bool has_factory() {
    return !!factory_;
  }

  // Returns true if the RenderViewHost instance is not a test instance.
  CONTENT_EXPORT static bool is_real_render_view_host() {
    return is_real_render_view_host_;
  }

  // Sets the is_real_render_view_host flag which indicates that the
  // RenderViewHost instance is not a test instance.
  CONTENT_EXPORT static void set_is_real_render_view_host(
      bool is_real_render_view_host) {
    is_real_render_view_host_ = is_real_render_view_host;
  }

 protected:
  RenderViewHostFactory() {}
  virtual ~RenderViewHostFactory() {}

  // You can derive from this class and specify an implementation for this
  // function to create a different kind of RenderViewHost for testing.
  virtual RenderViewHostImpl* CreateRenderViewHost(
      FrameTree* frame_tree,
      SiteInstanceGroup* group,
      const StoragePartitionConfig& storage_partition_config,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int32_t routing_id,
      int32_t main_frame_routing_id,
      int32_t widget_routing_id,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case,
      std::optional<viz::FrameSinkId> frame_sink_id) = 0;

  // Registers your factory to be called when new RenderViewHosts are created.
  // We have only one global factory, so there must be no factory registered
  // before the call. This class does NOT take ownership of the pointer.
  CONTENT_EXPORT static void RegisterFactory(RenderViewHostFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // the default RenderViewHosts will be created.
  CONTENT_EXPORT static void UnregisterFactory();

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default RenderViewHosts.
  CONTENT_EXPORT static RenderViewHostFactory* factory_;

  // Set to true if the RenderViewHost is not a test instance. Defaults to
  // false.
  CONTENT_EXPORT static bool is_real_render_view_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_
