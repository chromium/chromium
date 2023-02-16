// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_ui_data.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace extensions {

namespace {

content::GlobalRenderFrameHostId GetFrameRoutingId(
    content::RenderFrameHost* host) {
  if (!host)
    return content::GlobalRenderFrameHostId();

  return host->GetGlobalId();
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
          ExtensionApiFrameIdMap::GetDocumentLifecycle(navigation_handle)) {
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
          ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host)) {}

// static
std::unique_ptr<ExtensionNavigationUIData>
ExtensionNavigationUIData::CreateForMainFrameNavigation(
    content::WebContents* web_contents,
    int tab_id,
    int window_id) {
  return base::WrapUnique(new ExtensionNavigationUIData(
      web_contents, tab_id, window_id, ExtensionApiFrameIdMap::kTopFrameId,
      ExtensionApiFrameIdMap::kInvalidFrameId,
      content::GlobalRenderFrameHostId(),
      // Do not pass a valid document id in for the current document since
      // the current document isn't relevant to the new navigation.
      /*document_id=*/ExtensionApiFrameIdMap::DocumentId(),
      /*parent_document_id=*/ExtensionApiFrameIdMap::DocumentId(),
      api::extension_types::FRAME_TYPE_OUTERMOST_FRAME,
      api::extension_types::DOCUMENT_LIFECYCLE_ACTIVE));
}

std::unique_ptr<ExtensionNavigationUIData> ExtensionNavigationUIData::DeepCopy()
    const {
  auto copy = std::make_unique<ExtensionNavigationUIData>();
  copy->frame_data_ = frame_data_;
  copy->is_web_view_ = is_web_view_;
  copy->web_view_instance_id_ = web_view_instance_id_;
  copy->web_view_rules_registry_id_ = web_view_rules_registry_id_;
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
    api::extension_types::DocumentLifecycle document_lifecycle)
    : frame_data_(frame_id,
                  parent_frame_id,
                  tab_id,
                  window_id,
                  document_id,
                  parent_document_id,
                  frame_type,
                  document_lifecycle),
      parent_routing_id_(parent_routing_id) {
  WebViewGuest* web_view = WebViewGuest::FromWebContents(web_contents);
  if (web_view) {
    is_web_view_ = true;
    web_view_instance_id_ = web_view->view_instance_id();
    web_view_rules_registry_id_ = web_view->rules_registry_id();
  } else {
    is_web_view_ = false;
    web_view_instance_id_ = web_view_rules_registry_id_ = 0;
  }
}

}  // namespace extensions
