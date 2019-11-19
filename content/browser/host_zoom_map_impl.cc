// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace content {

namespace {

std::string GetHostFromProcessView(int render_process_id, int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return std::string();

  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);

  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry)
    return std::string();

  return net::GetHostOrSpecFromURL(HostZoomMap::GetURLFromEntry(entry));
}

}  // namespace

GURL HostZoomMap::GetURLFromEntry(NavigationEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (entry->GetPageType()) {
    case PAGE_TYPE_ERROR:
      return GURL(kUnreachableWebDataURL);
    // TODO(wjmaclean): In future, give interstitial pages special treatment as
    // well.
    default:
      return entry->GetURL();
  }
}

HostZoomMap* HostZoomMap::GetDefaultForBrowserContext(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(context);
  DCHECK(partition);
  return partition->GetHostZoomMap();
}

HostZoomMap* HostZoomMap::Get(SiteInstance* instance) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      instance->GetBrowserContext(), instance);
  DCHECK(partition);
  return partition->GetHostZoomMap();
}

HostZoomMap* HostZoomMap::GetForWebContents(WebContents* contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(wjmaclean): Update this behaviour to work with OOPIF.
  // See crbug.com/528407.
  StoragePartition* partition =
      BrowserContext::GetStoragePartition(contents->GetBrowserContext(),
                                          contents->GetSiteInstance());
  DCHECK(partition);
  return partition->GetHostZoomMap();
}

// Helper function for setting/getting zoom levels for WebContents without
// having to import HostZoomMapImpl everywhere.
double HostZoomMap::GetZoomLevel(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForWebContents(web_contents));
  return host_zoom_map->GetZoomLevelForWebContents(
      static_cast<WebContentsImpl*>(web_contents));
}

bool HostZoomMap::PageScaleFactorIsOne(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForWebContents(web_contents));
  return host_zoom_map->PageScaleFactorIsOneForWebContents(
      static_cast<WebContentsImpl*>(web_contents));
}

void HostZoomMap::SetZoomLevel(WebContents* web_contents, double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForWebContents(web_contents));
  host_zoom_map->SetZoomLevelForWebContents(
      static_cast<WebContentsImpl*>(web_contents), level);
}

void HostZoomMap::SendErrorPageZoomLevelRefresh(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::GetDefaultForBrowserContext(
          web_contents->GetBrowserContext()));
  host_zoom_map->SendErrorPageZoomLevelRefresh();
}

HostZoomMapImpl::HostZoomMapImpl()
    : default_zoom_level_(0.0),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void HostZoomMapImpl::CopyFrom(HostZoomMap* copy_interface) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMapImpl* copy = static_cast<HostZoomMapImpl*>(copy_interface);
  host_zoom_levels_.insert(copy->host_zoom_levels_.begin(),
                           copy->host_zoom_levels_.end());
  for (const auto& it : copy->scheme_host_zoom_levels_) {
    const std::string& host = it.first;
    scheme_host_zoom_levels_[host] = HostZoomLevels();
    scheme_host_zoom_levels_[host].insert(it.second.begin(), it.second.end());
  }
  default_zoom_level_ = copy->default_zoom_level_;
}

double HostZoomMapImpl::GetZoomLevelForHost(const std::string& host) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto it = host_zoom_levels_.find(host);
  return it != host_zoom_levels_.end() ? it->second.level : default_zoom_level_;
}

bool HostZoomMapImpl::HasZoomLevel(const std::string& scheme,
                                   const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto scheme_iterator(scheme_host_zoom_levels_.find(scheme));

  const HostZoomLevels& zoom_levels =
      (scheme_iterator != scheme_host_zoom_levels_.end())
          ? scheme_iterator->second
          : host_zoom_levels_;

  return base::Contains(zoom_levels, host);
}

double HostZoomMapImpl::GetZoomLevelForHostAndScheme(const std::string& scheme,
                                                     const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto scheme_iterator(scheme_host_zoom_levels_.find(scheme));
  if (scheme_iterator != scheme_host_zoom_levels_.end()) {
    auto i(scheme_iterator->second.find(host));
    if (i != scheme_iterator->second.end())
      return i->second.level;
  }

  return GetZoomLevelForHost(host);
}

HostZoomMap::ZoomLevelVector HostZoomMapImpl::GetAllZoomLevels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  HostZoomMap::ZoomLevelVector result;
  result.reserve(host_zoom_levels_.size() + scheme_host_zoom_levels_.size());
  for (const auto& entry : host_zoom_levels_) {
    ZoomLevelChange change = {
        HostZoomMap::ZOOM_CHANGED_FOR_HOST,
        entry.first,                // host
        std::string(),              // scheme
        entry.second.level,         // zoom level
        entry.second.last_modified  // last modified
    };
    result.push_back(change);
  }
  for (const auto& scheme_entry : scheme_host_zoom_levels_) {
    const std::string& scheme = scheme_entry.first;
    const HostZoomLevels& host_zoom_levels = scheme_entry.second;
    for (const auto& entry : host_zoom_levels) {
      ZoomLevelChange change = {
          HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST,
          entry.first,                // host
          scheme,                     // scheme
          entry.second.level,         // zoom level
          entry.second.last_modified  // last modified
      };
      result.push_back(change);
    }
  }
  return result;
}

void HostZoomMapImpl::SetZoomLevelForHost(const std::string& host,
                                          double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time last_modified = clock_->Now();
  SetZoomLevelForHostInternal(host, level, last_modified);
}

void HostZoomMapImpl::InitializeZoomLevelForHost(const std::string& host,
                                                 double level,
                                                 base::Time last_modified) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetZoomLevelForHostInternal(host, level, last_modified);
}

void HostZoomMapImpl::SetZoomLevelForHostInternal(const std::string& host,
                                                  double level,
                                                  base::Time last_modified) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (blink::PageZoomValuesEqual(level, default_zoom_level_)) {
    host_zoom_levels_.erase(host);
  } else {
    ZoomLevel& zoomLevel = host_zoom_levels_[host];
    zoomLevel.level = level;
    zoomLevel.last_modified = last_modified;
  }

  // TODO(wjmaclean) Should we use a GURL here? crbug.com/384486
  SendZoomLevelChange(std::string(), host);

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_HOST;
  change.host = host;
  change.zoom_level = level;
  change.last_modified = last_modified;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::SetZoomLevelForHostAndScheme(const std::string& scheme,
                                                   const std::string& host,
                                                   double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // No last_modified timestamp for scheme and host because they are
  // not persistet and are used for special cases only.
  scheme_host_zoom_levels_[scheme][host].level = level;

  SendZoomLevelChange(scheme, host);

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST;
  change.host = host;
  change.scheme = scheme;
  change.zoom_level = level;
  change.last_modified = base::Time();

  zoom_level_changed_callbacks_.Notify(change);
}

double HostZoomMapImpl::GetDefaultZoomLevel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return default_zoom_level_;
}

void HostZoomMapImpl::SetDefaultZoomLevel(double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (blink::PageZoomValuesEqual(level, default_zoom_level_))
    return;

  default_zoom_level_ = level;

  // First, remove all entries that match the new default zoom level.
  for (auto it = host_zoom_levels_.begin(); it != host_zoom_levels_.end();) {
    if (blink::PageZoomValuesEqual(it->second.level, default_zoom_level_))
      it = host_zoom_levels_.erase(it);
    else
      it++;
  }

  // Second, update zoom levels for all pages that do not have an overriding
  // entry.
  for (auto* web_contents : WebContentsImpl::GetAllWebContents()) {
    // Only change zoom for WebContents tied to the StoragePartition this
    // HostZoomMap serves.
    if (GetForWebContents(web_contents) != this)
      continue;

    int render_process_id =
        web_contents->GetRenderViewHost()->GetProcess()->GetID();
    int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();

    // Get the url from the navigation controller directly, as calling
    // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
    // is different than the one stored in the map.
    GURL url;
    std::string host;
    std::string scheme;

    NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    // It is possible for a WebContent's zoom level to be queried before
    // a navigation has occurred.
    if (entry) {
      url = GetURLFromEntry(entry);
      scheme = url.scheme();
      host = net::GetHostOrSpecFromURL(url);
    }

    bool uses_default_zoom =
        !HasZoomLevel(scheme, host) &&
        !UsesTemporaryZoomLevel(render_process_id, render_view_id);

    if (uses_default_zoom) {
      web_contents->UpdateZoom();

      HostZoomMap::ZoomLevelChange change;
      change.mode = HostZoomMap::ZOOM_CHANGED_FOR_HOST;
      change.host = host;
      change.zoom_level = level;

      zoom_level_changed_callbacks_.Notify(change);
    }
  }
}

std::unique_ptr<HostZoomMap::Subscription>
HostZoomMapImpl::AddZoomLevelChangedCallback(
    const ZoomLevelChangedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return zoom_level_changed_callbacks_.Add(callback);
}

double HostZoomMapImpl::GetZoomLevelForWebContents(
    WebContentsImpl* web_contents_impl) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int render_process_id =
      web_contents_impl->GetRenderViewHost()->GetProcess()->GetID();
  int routing_id = web_contents_impl->GetRenderViewHost()->GetRoutingID();

  if (UsesTemporaryZoomLevel(render_process_id, routing_id))
    return GetTemporaryZoomLevel(render_process_id, routing_id);

  // Get the url from the navigation controller directly, as calling
  // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
  // is different than is stored in the map.
  GURL url;
  NavigationEntry* entry =
      web_contents_impl->GetController().GetLastCommittedEntry();
  // It is possible for a WebContent's zoom level to be queried before
  // a navigation has occurred.
  if (entry)
    url = GetURLFromEntry(entry);
  return GetZoomLevelForHostAndScheme(url.scheme(),
                                      net::GetHostOrSpecFromURL(url));
}

void HostZoomMapImpl::SetZoomLevelForWebContents(
    WebContentsImpl* web_contents_impl,
    double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int render_process_id =
      web_contents_impl->GetRenderViewHost()->GetProcess()->GetID();
  int render_view_id = web_contents_impl->GetRenderViewHost()->GetRoutingID();
  if (UsesTemporaryZoomLevel(render_process_id, render_view_id)) {
    SetTemporaryZoomLevel(render_process_id, render_view_id, level);
  } else {
    // Get the url from the navigation controller directly, as calling
    // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
    // is different than what the render view is using. If the two don't match,
    // the attempt to set the zoom will fail.
    NavigationEntry* entry =
        web_contents_impl->GetController().GetLastCommittedEntry();
    // Tests may invoke this function with a null entry, but we don't
    // want to save zoom levels in this case.
    if (!entry)
      return;

    GURL url = GetURLFromEntry(entry);
    SetZoomLevelForHost(net::GetHostOrSpecFromURL(url), level);
  }
}

void HostZoomMapImpl::SetPageScaleFactorIsOneForView(int render_process_id,
                                                     int render_view_id,
                                                     bool is_one) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  view_page_scale_factors_are_one_[RenderViewKey(render_process_id,
                                                 render_view_id)] = is_one;
  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED;
  zoom_level_changed_callbacks_.Notify(change);
}

bool HostZoomMapImpl::PageScaleFactorIsOneForWebContents(
    WebContentsImpl* web_contents_impl) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents_impl->GetRenderViewHost()->GetProcess())
    return true;

  const auto it = view_page_scale_factors_are_one_.find(RenderViewKey(
      web_contents_impl->GetRenderViewHost()->GetProcess()->GetID(),
      web_contents_impl->GetRenderViewHost()->GetRoutingID()));
  return it != view_page_scale_factors_are_one_.end() ? it->second : true;
}

void HostZoomMapImpl::ClearPageScaleFactorIsOneForView(int render_process_id,
                                                       int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  view_page_scale_factors_are_one_.erase(
      RenderViewKey(render_process_id, render_view_id));
}

bool HostZoomMapImpl::UsesTemporaryZoomLevel(int render_process_id,
                                             int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderViewKey key(render_process_id, render_view_id);
  return base::Contains(temporary_zoom_levels_, key);
}

double HostZoomMapImpl::GetTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderViewKey key(render_process_id, render_view_id);
  const auto it = temporary_zoom_levels_.find(key);
  return it != temporary_zoom_levels_.end() ? it->second : 0;
}

void HostZoomMapImpl::SetTemporaryZoomLevel(int render_process_id,
                                            int render_view_id,
                                            double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderViewKey key(render_process_id, render_view_id);
  temporary_zoom_levels_[key] = level;

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          RenderViewHost::FromID(render_process_id, render_view_id)));
  web_contents->UpdateZoom();

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM;
  change.host = GetHostFromProcessView(render_process_id, render_view_id);
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::ClearZoomLevels(base::Time delete_begin,
                                      base::Time delete_end) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  double default_zoom_level = GetDefaultZoomLevel();
  for (const auto& zoom_level : GetAllZoomLevels()) {
    if (zoom_level.scheme.empty() && delete_begin <= zoom_level.last_modified &&
        (delete_end.is_null() || zoom_level.last_modified < delete_end)) {
      SetZoomLevelForHost(zoom_level.host, default_zoom_level);
    }
  }
}

void HostZoomMapImpl::ClearTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderViewKey key(render_process_id, render_view_id);
  auto it = temporary_zoom_levels_.find(key);
  if (it == temporary_zoom_levels_.end())
    return;

  temporary_zoom_levels_.erase(it);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          RenderViewHost::FromID(render_process_id, render_view_id)));
  web_contents->UpdateZoom();
}

void HostZoomMapImpl::SendZoomLevelChange(const std::string& scheme,
                                          const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We'll only send to WebContents not using temporary zoom levels. The one
  // other case of interest is where the renderer is hosting a plugin document;
  // that should be reflected in our temporary zoom level map, but we will
  // double check on the renderer side to avoid the possibility of any races.
  for (auto* web_contents : WebContentsImpl::GetAllWebContents()) {
    // Only send zoom level changes to WebContents that are using this
    // HostZoomMap.
    if (GetForWebContents(web_contents) != this)
      continue;

    int render_process_id =
        web_contents->GetRenderViewHost()->GetProcess()->GetID();
    int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();

    if (!UsesTemporaryZoomLevel(render_process_id, render_view_id))
      web_contents->UpdateZoomIfNecessary(scheme, host);
  }
}

void HostZoomMapImpl::SendErrorPageZoomLevelRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL error_url(kUnreachableWebDataURL);
  std::string host = net::GetHostOrSpecFromURL(error_url);

  SendZoomLevelChange(std::string(), host);
}

void HostZoomMapImpl::WillCloseRenderView(int render_process_id,
                                          int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearTemporaryZoomLevel(render_process_id, render_view_id);
  ClearPageScaleFactorIsOneForView(render_process_id, render_view_id);
}

HostZoomMapImpl::~HostZoomMapImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void HostZoomMapImpl::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace content
