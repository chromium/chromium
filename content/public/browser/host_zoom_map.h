// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
#define CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
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
class WebContents;

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
    PAGE_SCALE_IS_ONE_CHANGED,         // Page scale factor equal to one changed
                                       // for a host.
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

  // Returns the current zoom level for the specified WebContents. May be
  // temporary or host-specific.
  CONTENT_EXPORT static double GetZoomLevel(WebContents* web_contents);

  // Returns true if the page scale factor for the WebContents is one.
  CONTENT_EXPORT static bool PageScaleFactorIsOne(WebContents* web_contents);

  // Sets the current zoom level for the specified WebContents. The level may
  // be temporary or host-specific depending on the particular WebContents.
  CONTENT_EXPORT static void SetZoomLevel(WebContents* web_contents,
                                          double level);

  // Send an IPC to refresh any displayed error page's zoom levels. Needs to
  // be called since error pages don't get loaded via the normal channel.
  CONTENT_EXPORT static void SendErrorPageZoomLevelRefresh(
      WebContents* web_contents);

  // Set or clear whether or not the page scale factor for a view is one.
  virtual void SetPageScaleFactorIsOneForView(
      int render_process_id, int render_view_id, bool is_one) = 0;
  virtual void ClearPageScaleFactorIsOneForView(
      int render_process_id, int render_view_id) = 0;

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

  // Returns whether the view manages its zoom level independently of other
  // views displaying content from the same host.
  virtual bool UsesTemporaryZoomLevel(int render_process_id,
                                      int render_view_id) = 0;

  // Sets the temporary zoom level that's only valid for the lifetime of this
  // WebContents.
  //
  // This should only be called on the UI thread.
  virtual void SetTemporaryZoomLevel(int render_process_id,
                                     int render_view_id,
                                     double level) = 0;

  // Clear zoom levels with a modification date greater than or equal
  // to |delete_begin| and less than |delete_end|. If |delete_end| is null,
  // all entries after |delete_begin| will be deleted.
  virtual void ClearZoomLevels(base::Time delete_begin,
                               base::Time delete_end) = 0;

  // Clears the temporary zoom level stored for this WebContents.
  //
  // This should only be called on the UI thread.
  virtual void ClearTemporaryZoomLevel(int render_process_id,
                                       int render_view_id) = 0;

  // Get/Set the default zoom level for pages that don't override it.
  virtual double GetDefaultZoomLevel() = 0;
  virtual void SetDefaultZoomLevel(double level) = 0;

  typedef base::Callback<void(const ZoomLevelChange&)> ZoomLevelChangedCallback;
  typedef base::CallbackList<void(const ZoomLevelChange&)>::Subscription
      Subscription;
  // Add and remove zoom level changed callbacks.
  virtual std::unique_ptr<Subscription> AddZoomLevelChangedCallback(
      const ZoomLevelChangedCallback& callback) = 0;

  virtual void SetClockForTesting(base::Clock* clock) = 0;

 protected:
  virtual ~HostZoomMap() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
