// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_
#define EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_

#include <memory>
#include <optional>

#include "content/public/browser/global_routing_id.h"
#include "extensions/browser/extension_api_frame_id_map.h"

namespace content {
class NavigationHandle;
}

namespace extensions {

// Initialized on the UI thread for all navigations. A copy is used on the IO
// thread by the WebRequest API to access to the FrameData.
class ExtensionNavigationUIData {
 public:
  ExtensionNavigationUIData();
  ExtensionNavigationUIData(content::NavigationHandle* navigation_handle,
                            int tab_id,
                            int window_id);
  ExtensionNavigationUIData(content::RenderFrameHost* frame_host,
                            int tab_id,
                            int window_id);

  ExtensionNavigationUIData(const ExtensionNavigationUIData&) = delete;
  ExtensionNavigationUIData& operator=(const ExtensionNavigationUIData&) =
      delete;

  static std::unique_ptr<ExtensionNavigationUIData>
  CreateForMainFrameNavigation(content::WebContents* web_contents,
                               int tab_id,
                               int window_id);

  std::unique_ptr<ExtensionNavigationUIData> DeepCopy() const;

  const ExtensionApiFrameIdMap::FrameData& frame_data() const {
    return frame_data_;
  }

  struct WebViewData {
    int web_view_instance_id = 0;
    int web_view_rules_registry_id = 0;
  };

  bool is_web_view() const { return web_view_data_.has_value(); }
  int web_view_instance_id() const {
    return web_view_data_->web_view_instance_id;
  }
  int web_view_rules_registry_id() const {
    return web_view_data_->web_view_rules_registry_id;
  }

  const content::GlobalRenderFrameHostId& parent_routing_id() const {
    return parent_routing_id_;
  }

 private:
  ExtensionNavigationUIData(
      content::WebContents* web_contents,
      int tab_id,
      int window_id,
      int frame_id,
      int parent_frame_id,
      content::GlobalRenderFrameHostId parent_routing_id,
      const ExtensionApiFrameIdMap::DocumentId& document_id,
      const ExtensionApiFrameIdMap::DocumentId& parent_document_id,
      api::extension_types::FrameType frame_type,
      api::extension_types::DocumentLifecycle document_lifecycle,
      std::optional<WebViewData> web_view_data);

  ExtensionApiFrameIdMap::FrameData frame_data_;
  std::optional<WebViewData> web_view_data_;

  // ID for the parent RenderFrameHost of this navigation. Will only have a
  // valid value for sub-frame navigations.
  content::GlobalRenderFrameHostId parent_routing_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_UI_DATA_H_
