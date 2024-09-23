// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
#define CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace content {

class NavigationEntry;
class BrowserContext;
class SiteInstance;
class StoragePartition;
class WebContents;
struct GlobalRenderFrameHostId;

// Maps hostnames to custom zoom levels.  Written on the UI thread and read on
// any thread.  One instance per browser context. Must be created on the UI
// thread, and it'll delete itself on the UI thread as well.
// Zoom can be defined at three levels: default zoom, zoom for host, and zoom
// for host with specific scheme. Setting any of the levels leaves settings
// for other settings intact. Getting the zoom level starts at the most
// specific setting and progresses to the less specific: first the zoom for the
// host and scheme pair is checked, secondly the zoom for the host only and
// lastly default zoom.

class HostZoomMap {
 public:
  // Enum that indicates what was the scope of zoom level change.
  enum ZoomLevelChangeMode {
    ZOOM_CHANGED_FOR_HOST,             // Zoom level changed for host.
    ZOOM_CHANGED_FOR_SCHEME_AND_HOST,  // Zoom level changed for scheme/host
                                       // pair.
    ZOOM_CHANGED_TEMPORARY_ZOOM,       // Temporary zoom change for specific
                                       // renderer, no scheme/host is specified.
  };

  // Structure used to notify about zoom changes. Host and/or scheme are empty
  // if not applicable to |mode|.
  struct ZoomLevelChange {
    ZoomLevelChangeMode mode;
    std::string host;
    std::string scheme;
    double zoom_level;
    base::Time last_modified;
  };

  typedef std::vector<ZoomLevelChange> ZoomLevelVector;

  // Extracts the URL from NavigationEntry, substituting the error page
  // URL in the event that the error page is showing.
  CONTENT_EXPORT static GURL GetURLFromEntry(NavigationEntry* entry);

  CONTENT_EXPORT static HostZoomMap* GetDefaultForBrowserContext(
      BrowserContext* browser_context);

  // Returns the HostZoomMap associated with this SiteInstance. The SiteInstance
  // may serve multiple WebContents, and the HostZoomMap is the same for all of
  // these WebContents.
  CONTENT_EXPORT static HostZoomMap* Get(SiteInstance* instance);

  // Returns the HostZoomMap associated with this WebContent's main frame. If
  // multiple WebContents share the same SiteInstance, then they share a single
  // HostZoomMap.
  CONTENT_EXPORT static HostZoomMap* GetForWebContents(WebContents* contents);

  // Returns the HostZoomMap associated with this StoragePartition.
  CONTENT_EXPORT static HostZoomMap* GetForStoragePartition(
      StoragePartition* storage_partition);

  // Returns the current zoom level for the specified WebContents. May be
  // temporary or host-specific.
  CONTENT_EXPORT static double GetZoomLevel(WebContents* web_contents);

  // Sets the current zoom level for the specified WebContents. The level may
  // be temporary or host-specific depending on the particular WebContents.
  CONTENT_EXPORT static void SetZoomLevel(WebContents* web_contents,
                                          double level);

  // Send an IPC to refresh any displayed error page's zoom levels. Needs to
  // be called since error pages don't get loaded via the normal channel.
  CONTENT_EXPORT static void SendErrorPageZoomLevelRefresh(
      WebContents* web_contents);

  // Copy the zoom levels from the given map. Can only be called on the UI
  // thread.
  virtual void CopyFrom(HostZoomMap* copy) = 0;

  // Here |host| is the host portion of URL, or (in the absence of a host)
  // the complete spec of the URL.
  // Returns the zoom for the specified |scheme| and |host|. See class
  // description for details.
  //
  // This may be called on any thread.
  virtual double GetZoomLevelForHostAndScheme(const std::string& scheme,
                                              const std::string& host) = 0;

  // Returns true if the specified |scheme| and/or |host| has a zoom level
  // currently set.
  //
  // This may be called on any thread.
  virtual bool HasZoomLevel(const std::string& scheme,
                            const std::string& host) = 0;

  // Returns all non-temporary zoom levels. Can be called on any thread.
  virtual ZoomLevelVector GetAllZoomLevels() = 0;

  // Here |host| is the host portion of URL, or (in the absence of a host)
  // the complete spec of the URL.
  // Sets the zoom level for the |host| to |level|.  If the level matches the
  // current default zoom level, the host is erased from the saved preferences;
  // otherwise the new value is written out.
  // Zoom levels specified for both scheme and host are not affected.
  //
  // This should only be called on the UI thread.
  virtual void SetZoomLevelForHost(const std::string& host, double level) = 0;

  // Sets the zoom level for the |host| to |level| with a given |last_modified|
  // timestamp. Should only be used for initialization.
  virtual void InitializeZoomLevelForHost(const std::string& host,
                                          double level,
                                          base::Time last_modified) = 0;

  // Here |host| is the host portion of URL, or (in the absence of a host)
  // the complete spec of the URL.
  // Sets the zoom level for the |scheme|/|host| pair to |level|. No values
  // will be erased during this operation, and this value will not be stored in
  // the preferences.
  //
  // This should only be called on the UI thread.
  virtual void SetZoomLevelForHostAndScheme(const std::string& scheme,
                                            const std::string& host,
                                            double level) = 0;

  // Returns whether the frame manages its zoom level independently of other
  // frames from the same host.
  virtual bool UsesTemporaryZoomLevel(
      const GlobalRenderFrameHostId& rfh_id) = 0;

  // Sets the temporary zoom level that's only valid for the lifetime of this
  // RenderFrameHost.
  //
  // This should only be called on the UI thread.
  virtual void SetTemporaryZoomLevel(const GlobalRenderFrameHostId& rfh_id,
                                     double level) = 0;

  // Clear zoom levels with a modification date greater than or equal
  // to |delete_begin| and less than |delete_end|. If |delete_end| is null,
  // all entries after |delete_begin| will be deleted.
  virtual void ClearZoomLevels(base::Time delete_begin,
                               base::Time delete_end) = 0;

  // Clears the temporary zoom level stored for this RenderFrameHost.
  //
  // This should only be called on the UI thread.
  virtual void ClearTemporaryZoomLevel(
      const GlobalRenderFrameHostId& rfh_id) = 0;

  // Get/Set the default zoom level for pages that don't override it.
  virtual double GetDefaultZoomLevel() = 0;
  virtual void SetDefaultZoomLevel(double level) = 0;

  using ZoomLevelChangedCallback =
      base::RepeatingCallback<void(const ZoomLevelChange&)>;
  // Add and remove zoom level changed callbacks.
  virtual base::CallbackListSubscription AddZoomLevelChangedCallback(
      ZoomLevelChangedCallback callback) = 0;

  virtual void SetClockForTesting(base::Clock* clock) = 0;

  // On Android only, set a callback for when the Java-side UI sets a default
  // zoom level so the HostZoomMapImpl does not depend on Prefs or //chrome/.
#if BUILDFLAG(IS_ANDROID)
  using DefaultZoomChangedCallback =
      base::RepeatingCallback<void(double new_level)>;

  virtual void SetDefaultZoomLevelPrefCallback(
      DefaultZoomChangedCallback callback) = 0;

  // TODO(crbug.com/40898422): Make an Android-specific impl of host_zoom_map,
  // or
  //                          combine method with GetZoomLevelForHostAndScheme.
  virtual double GetZoomLevelForHostAndSchemeAndroid(
      const std::string& scheme,
      const std::string& host) = 0;
#endif

  // Accessors for preview
  //
  // Zoom levels for preview are isolated from normal ones, stored to memory
  // only in a session and not persisted to prefs.
  //
  // See also `PreviewZoomController`.
  //
  // In long-term, we are planning to persist zoom levels for preview as same as
  // normal ones. An option is adding HostZoomMapImpl::is_for_preview_ and
  // another instance of HostZoomMapImpl to StoragePartition via
  // HostZoomLevelContext. In short-term, we tihs is not appropriate and we
  // tentatively use HostZoomMapImpl.
  //
  // TODO(b:315313138): Revisit here and redesign it.
  virtual double GetZoomLevelForPreviewAndHost(const std::string& host) = 0;
  virtual void SetZoomLevelForPreviewAndHost(const std::string& host,
                                             double level) = 0;

 protected:
  virtual ~HostZoomMap() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
