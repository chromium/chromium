// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "content/public/browser/android/browser_context_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/HostZoomMapImpl_jni.h"
#endif

namespace content {

namespace {

std::string GetHostFromProcessFrame(RenderFrameHostImpl* rfh) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!rfh)
    return std::string();

  NavigationEntry* entry =
      rfh->frame_tree()->controller().GetLastCommittedEntry();
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

// static
HostZoomMap* HostZoomMap::GetDefaultForBrowserContext(BrowserContext* context) {
  return GetForStoragePartition(context->GetDefaultStoragePartition());
}

// static
HostZoomMap* HostZoomMap::Get(SiteInstance* instance) {
  return GetForStoragePartition(
      instance->GetBrowserContext()->GetStoragePartition(instance));
}

// static
HostZoomMap* HostZoomMap::GetForWebContents(WebContents* contents) {
  // TODO(wjmaclean): Update this behaviour to work with OOPIF.
  // See crbug.com/528407.
  return Get(contents->GetSiteInstance());
}

// static
HostZoomMap* HostZoomMap::GetForStoragePartition(
    StoragePartition* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(storage_partition);
  return storage_partition->GetHostZoomMap();
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

  host_zoom_levels_for_preview_.insert(
      copy->host_zoom_levels_for_preview_.begin(),
      copy->host_zoom_levels_for_preview_.end());
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

#if BUILDFLAG(IS_ANDROID)
double HostZoomMapImpl::GetZoomLevelForHostAndSchemeAndroid(
    const std::string& scheme,
    const std::string& host) {
  double zoom_level = GetZoomLevelForHostAndScheme(scheme, host);

  // On Android, we will use a zoom level that considers the current OS-level
  // setting. For this we pass the given |level| through JNI to the Java-side
  // code, which can access the Android configuration and |fontScale|. This
  // method will return the adjusted zoom level considering OS settings. Note
  // that the OS |fontScale| will be factored in only when the Page Zoom feature
  // is enabled.
  JNIEnv* env = jni_zero::AttachCurrentThread();
  double adjusted_zoom_level =
      Java_HostZoomMapImpl_getAdjustedZoomLevel(env, zoom_level);
  return adjusted_zoom_level;
}
#endif

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

  if (blink::ZoomValuesEqual(level, default_zoom_level_)) {
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

  if (blink::ZoomValuesEqual(level, default_zoom_level_)) {
    return;
  }

  default_zoom_level_ = level;

  // First, remove all entries that match the new default zoom level.
  for (auto it = host_zoom_levels_.begin(); it != host_zoom_levels_.end();) {
    if (blink::ZoomValuesEqual(it->second.level, default_zoom_level_)) {
      it = host_zoom_levels_.erase(it);
    } else {
      it++;
    }
  }

  // Second, update zoom levels for all pages that do not have an overriding
  // entry.
  for (auto* web_contents : WebContentsImpl::GetAllWebContents()) {
    // Only change zoom for WebContents tied to the StoragePartition this
    // HostZoomMap serves.
    if (GetForWebContents(web_contents) != this)
      continue;

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
        !UsesTemporaryZoomLevel(
            web_contents->GetPrimaryMainFrame()->GetGlobalId());

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

base::CallbackListSubscription HostZoomMapImpl::AddZoomLevelChangedCallback(
    ZoomLevelChangedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return zoom_level_changed_callbacks_.Add(std::move(callback));
}

double HostZoomMapImpl::GetZoomLevelForWebContents(
    WebContentsImpl* web_contents_impl) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GlobalRenderFrameHostId rfh_id =
      web_contents_impl->GetPrimaryMainFrame()->GetGlobalId();
  if (UsesTemporaryZoomLevel(rfh_id))
    return GetTemporaryZoomLevel(rfh_id);

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

#if BUILDFLAG(IS_ANDROID)
  return GetZoomLevelForHostAndSchemeAndroid(url.scheme(),
                                             net::GetHostOrSpecFromURL(url));
#else
  return GetZoomLevelForHostAndScheme(url.scheme(),
                                      net::GetHostOrSpecFromURL(url));
#endif
}

void HostZoomMapImpl::SetZoomLevelForWebContents(
    WebContentsImpl* web_contents_impl,
    double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GlobalRenderFrameHostId rfh_id =
      web_contents_impl->GetPrimaryMainFrame()->GetGlobalId();
  if (UsesTemporaryZoomLevel(rfh_id)) {
    SetTemporaryZoomLevel(rfh_id, level);
  } else {
    // Get the url from the navigation controller directly, as calling
    // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
    // is different than what the render frame is using. If the two don't match,
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

bool HostZoomMapImpl::UsesTemporaryZoomLevel(
    const GlobalRenderFrameHostId& rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Contains(temporary_zoom_levels_, rfh_id);
}

void HostZoomMapImpl::SetNoLongerUsesTemporaryZoomLevel(
    const GlobalRenderFrameHostId& rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  temporary_zoom_levels_.erase(rfh_id);
}

double HostZoomMapImpl::GetTemporaryZoomLevel(
    const GlobalRenderFrameHostId& rfh_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto it = temporary_zoom_levels_.find(rfh_id);

  return it != temporary_zoom_levels_.end() ? it->second : 0;
}

void HostZoomMapImpl::SetTemporaryZoomLevel(
    const GlobalRenderFrameHostId& rfh_id,
    double level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(rfh_id);
  DCHECK(rfh == rfh->GetOutermostMainFrame());

  temporary_zoom_levels_[rfh_id] = level;

  WebContentsImpl* web_contents = WebContentsImpl::FromRenderFrameHostImpl(rfh);
  web_contents->UpdateZoom();

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM;
  change.host = GetHostFromProcessFrame(rfh);
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

void HostZoomMapImpl::ClearTemporaryZoomLevel(
    const GlobalRenderFrameHostId& rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = temporary_zoom_levels_.find(rfh_id);
  if (it == temporary_zoom_levels_.end())
    return;

  temporary_zoom_levels_.erase(it);
  WebContentsImpl* web_contents = WebContentsImpl::FromRenderFrameHostImpl(
      RenderFrameHostImpl::FromID(rfh_id));
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

    if (!UsesTemporaryZoomLevel(
            web_contents->GetPrimaryMainFrame()->GetGlobalId())) {
      web_contents->UpdateZoomIfNecessary(scheme, host);
    }
  }
}

void HostZoomMapImpl::SendErrorPageZoomLevelRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL error_url(kUnreachableWebDataURL);
  std::string host = net::GetHostOrSpecFromURL(error_url);

  SendZoomLevelChange(std::string(), host);
}

HostZoomMapImpl::~HostZoomMapImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void HostZoomMapImpl::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

#if BUILDFLAG(IS_ANDROID)
void HostZoomMapImpl::SetSystemFontScaleForTesting(float scale) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_HostZoomMapImpl_setSystemFontScaleForTesting(env, scale);  // IN-TEST
}

void HostZoomMapImpl::SetDefaultZoomLevelPrefCallback(
    HostZoomMap::DefaultZoomChangedCallback callback) {
  default_zoom_level_pref_callback_ = std::move(callback);
}

HostZoomMap::DefaultZoomChangedCallback*
HostZoomMapImpl::GetDefaultZoomLevelPrefCallback() {
  return &default_zoom_level_pref_callback_;
}

void JNI_HostZoomMapImpl_SetZoomLevel(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jdouble new_zoom_level,
    jdouble adjusted_zoom_level) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();

  // We want to set and save the new zoom level, but we want to actually render
  // the adjusted level.
  HostZoomMap::SetZoomLevel(web_contents, new_zoom_level);

  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForWebContents(web_contents));
  host_zoom_map->SetTemporaryZoomLevel(rfh_id, adjusted_zoom_level);

  // We must now remove this webcontents from the list of temporary zoom levels,
  // this is so that any future request will continue to update the underlying
  // host/scheme save, and will not be perceived as "temporary".
  // i.e. once temporary is set for a web_contents, the call to
  // SetZoomLevelForWebContents will keep updating what is rendered, but will no
  // longer call SetZoomLevelForHost, which saves the choice for that host.
  host_zoom_map->SetNoLongerUsesTemporaryZoomLevel(rfh_id);
}

void JNI_HostZoomMapImpl_SetZoomLevelForHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_context,
    const base::android::JavaParamRef<jstring>& j_host,
    jdouble level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* context = BrowserContextFromJavaHandle(j_context);
  if (!context) {
    return;
  }

  HostZoomMap* host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(context);

  std::string host(base::android::ConvertJavaStringToUTF8(env, j_host));
  host_zoom_map->SetZoomLevelForHost(host, level);
}

jdouble JNI_HostZoomMapImpl_GetZoomLevel(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  return HostZoomMap::GetZoomLevel(web_contents);
}

void JNI_HostZoomMapImpl_SetDefaultZoomLevel(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_context,
    jdouble new_default_zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* context = BrowserContextFromJavaHandle(j_context);
  if (!context)
    return;

  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetDefaultForBrowserContext(context));

  // If a callback has been set (e.g. by chrome_zoom_level_prefs to store an
  // updated value in Prefs), call this now with the chosen zoom level.
  if (host_zoom_map->GetDefaultZoomLevelPrefCallback()) {
    host_zoom_map->GetDefaultZoomLevelPrefCallback()->Run(
        new_default_zoom_level);
  }

  // Update the default zoom level for existing tabs. This must be done after
  // the Pref is updated due to guard clause in chrome_zoom_level_prefs.
  host_zoom_map->SetDefaultZoomLevel(new_default_zoom_level);
}

jdouble JNI_HostZoomMapImpl_GetDefaultZoomLevel(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* context = BrowserContextFromJavaHandle(j_context);
  if (!context)
    return 0.0;

  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetDefaultForBrowserContext(context));
  return host_zoom_map->GetDefaultZoomLevel();
}

std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_HostZoomMapImpl_GetAllHostZoomLevels(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ret;

  // Get instance of HostZoomMap.
  BrowserContext* context = BrowserContextFromJavaHandle(j_context);
  if (!context) {
    return ret;
  }

  HostZoomMap* host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(context);

  // Convert C++ vector of structs to vector of objects.
  for (const auto& entry : host_zoom_map->GetAllZoomLevels()) {
    switch (entry.mode) {
      case HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        ret.push_back(Java_HostZoomMapImpl_buildSiteZoomInfo(env, entry.host,
                                                             entry.zoom_level));
        break;
      }
      case HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        NOTREACHED_IN_MIGRATION();
        break;
      case HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED_IN_MIGRATION();
    }
  }

  return ret;
}
#endif

double HostZoomMapImpl::GetZoomLevelForPreviewAndHost(const std::string& host) {
  const auto it = host_zoom_levels_for_preview_.find(host);
  return it != host_zoom_levels_for_preview_.end() ? it->second.level
                                                   : default_zoom_level_;
}

void HostZoomMapImpl::SetZoomLevelForPreviewAndHost(const std::string& host,
                                                    double level) {
  if (blink::ZoomValuesEqual(level, default_zoom_level_)) {
    host_zoom_levels_for_preview_.erase(host);
  } else {
    ZoomLevel& zoomLevel = host_zoom_levels_for_preview_[host];
    zoomLevel.level = level;
    zoomLevel.last_modified = clock_->Now();
  }
}

}  // namespace content
