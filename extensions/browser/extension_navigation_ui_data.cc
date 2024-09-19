// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_ui_data.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

namespace extensions {

namespace {

content::GlobalRenderFrameHostId GetFrameRoutingId(
    content::RenderFrameHost* host) {
  if (!host) {
    return content::GlobalRenderFrameHostId();
  }

  return host->GetGlobalId();
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
std::optional<ExtensionNavigationUIData::WebViewData> GetWebViewData(
    WebViewGuest* web_view) {
  if (!web_view) {
    return {};
  }

  ExtensionNavigationUIData::WebViewData web_view_data;
  web_view_data.web_view_instance_id = web_view->view_instance_id();
  web_view_data.web_view_rules_registry_id = web_view->rules_registry_id();
  return web_view_data;
}
#endif

std::optional<ExtensionNavigationUIData::WebViewData> GetWebViewData(
    content::NavigationHandle* navigation_handle) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  return GetWebViewData(WebViewGuest::FromNavigationHandle(navigation_handle));
#else
  return {};
#endif
}

std::optional<ExtensionNavigationUIData::WebViewData> GetWebViewData(
    content::RenderFrameHost* frame_host) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  return GetWebViewData(WebViewGuest::FromRenderFrameHost(frame_host));
#else
  return {};
#endif
}

}  // namespace

ExtensionNavigationUIData::ExtensionNavigationUIData() = default;

ExtensionNavigationUIData::ExtensionNavigationUIData(
    content::NavigationHandle* navigation_handle,
    int tab_id,
    int window_id)
    : ExtensionNavigationUIData(
          navigation_handle->GetWebContents(),
          tab_id,
          window_id,
          ExtensionApiFrameIdMap::GetFrameId(navigation_handle),
          ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle),
          GetFrameRoutingId(navigation_handle->GetParentFrameOrOuterDocument()),
          // Do not pass a valid document id in for the current document since
          // the current document isn't relevant to the new navigation.
          /*document_id=*/ExtensionApiFrameIdMap::DocumentId(),
          /*parent_document_id=*/
          ExtensionApiFrameIdMap::GetDocumentId(
              navigation_handle->GetParentFrameOrOuterDocument()),
          ExtensionApiFrameIdMap::GetFrameType(navigation_handle),
          ExtensionApiFrameIdMap::GetDocumentLifecycle(navigation_handle),
          GetWebViewData(navigation_handle)) {
  // TODO(clamy): See if it would be possible to have just one source for the
  // FrameData that works both for navigations and subresources loads.
}

ExtensionNavigationUIData::ExtensionNavigationUIData(
    content::RenderFrameHost* frame_host,
    int tab_id,
    int window_id)
    : ExtensionNavigationUIData(
          content::WebContents::FromRenderFrameHost(frame_host),
          tab_id,
          window_id,
          ExtensionApiFrameIdMap::GetFrameId(frame_host),
          ExtensionApiFrameIdMap::GetParentFrameId(frame_host),
          GetFrameRoutingId(frame_host->GetParentOrOuterDocument()),
          // Do not pass a valid document id in for the current document since
          // the current document isn't relevant to the new navigation.
          /*document_id=*/ExtensionApiFrameIdMap::DocumentId(),
          /*parent_document_id=*/
          ExtensionApiFrameIdMap::GetDocumentId(
              frame_host->GetParentOrOuterDocument()),
          ExtensionApiFrameIdMap::GetFrameType(frame_host),
          ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host),
          GetWebViewData(frame_host)) {}

// static
std::unique_ptr<ExtensionNavigationUIData>
ExtensionNavigationUIData::CreateForMainFrameNavigation(
    content::WebContents* web_contents,
    int tab_id,
    int window_id) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  CHECK(!WebViewGuest::FromWebContents(web_contents));
#endif
  return base::WrapUnique(new ExtensionNavigationUIData(
      web_contents, tab_id, window_id, ExtensionApiFrameIdMap::kTopFrameId,
      ExtensionApiFrameIdMap::kInvalidFrameId,
      content::GlobalRenderFrameHostId(),
      // Do not pass a valid document id in for the current document since
      // the current document isn't relevant to the new navigation.
      /*document_id=*/ExtensionApiFrameIdMap::DocumentId(),
      /*parent_document_id=*/ExtensionApiFrameIdMap::DocumentId(),
      api::extension_types::FrameType::kOutermostFrame,
      api::extension_types::DocumentLifecycle::kActive,
      /*web_view_data=*/std::nullopt));
}

std::unique_ptr<ExtensionNavigationUIData> ExtensionNavigationUIData::DeepCopy()
    const {
  auto copy = std::make_unique<ExtensionNavigationUIData>();
  copy->frame_data_ = frame_data_;
  copy->web_view_data_ = web_view_data_;
  copy->parent_routing_id_ = parent_routing_id_;
  return copy;
}

ExtensionNavigationUIData::ExtensionNavigationUIData(
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
    std::optional<WebViewData> web_view_data)
    : frame_data_(frame_id,
                  parent_frame_id,
                  tab_id,
                  window_id,
                  document_id,
                  parent_document_id,
                  frame_type,
                  document_lifecycle),
      web_view_data_(web_view_data),
      parent_routing_id_(parent_routing_id) {}

}  // namespace extensions
