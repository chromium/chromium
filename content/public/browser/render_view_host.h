// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_zoom.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/system/core.h"
#include "third_party/blink/public/platform/web_drag_operation.h"

namespace base {
class FilePath;
}

namespace blink {
struct WebPluginAction;
}

namespace gfx {
class Point;
}

namespace content {

class RenderFrameHost;
class RenderProcessHost;
class RenderViewHostDelegate;
class RenderWidgetHost;
class SiteInstance;
struct WebPreferences;

// A RenderViewHost is responsible for creating and talking to a RenderView
// object in a child process. It exposes a high level API to users, for things
// like loading pages, adjusting the display and other browser functionality,
// which it translates into IPC messages sent over the IPC channel with the
// RenderView. It responds to all IPC messages sent by that RenderView and
// cracks them, calling a delegate object back with higher level types where
// possible.
//
// The intent of this interface is to provide a view-agnostic communication
// conduit with a renderer. This is so we can build HTML views not only as
// WebContents (see WebContents for an example) but also as views, etc.
//
// DEPRECATED: RenderViewHost is being removed as part of the SiteIsolation
// project. New code should not be added here, but to RenderWidgetHost (if it's
// about drawing or events), RenderFrameHost (if it's frame specific), or
// WebContents (if it's page specific).
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHost : public IPC::Sender {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // Returns the RenderViewHost, if any, that uses the specified
  // RenderWidgetHost. Returns nullptr if there is no such RenderViewHost.
  static RenderViewHost* From(RenderWidgetHost* rwh);

  ~RenderViewHost() override {}

  // Returns the RenderWidgetHost for this RenderViewHost.
  virtual RenderWidgetHost* GetWidget() const = 0;

  // Returns the RenderProcessHost for this RenderViewHost.
  virtual RenderProcessHost* GetProcess() const = 0;

  // Returns the routing id for IPC use for this RenderViewHost.
  //
  // Implementation note: Historically, RenderViewHost was-a RenderWidgetHost,
  // and shared its IPC channel and its routing ID. Although this inheritance is
  // no longer so, the IPC channel is currently still shared. Expect this to
  // change.
  virtual int GetRoutingID() const = 0;

  // Returns the main frame for this render view.
  virtual RenderFrameHost* GetMainFrame() = 0;

  // Notifies the listener that a directory enumeration is complete.
  virtual void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) = 0;

  // Instructs the RenderView to send back updates to the preferred size.
  virtual void EnablePreferredSizeMode() = 0;

  // Tells the renderer to perform the given action on the plugin located at
  // the given point.
  virtual void ExecutePluginActionAtLocation(
      const gfx::Point& location, const blink::WebPluginAction& action) = 0;

  virtual RenderViewHostDelegate* GetDelegate() const = 0;

  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns true if the RenderView is active and has not crashed.
  virtual bool IsRenderViewLive() const = 0;

  // Notification that a move or resize renderer's containing window has
  // started.
  virtual void NotifyMoveOrResizeStarted() = 0;

  // Sets a property with the given name and value on the Web UI binding object.
  // Must call AllowWebUIBindings() on this renderer first.
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) = 0;

  // Send the renderer process the current preferences supplied by the
  // RenderViewHostDelegate.
  virtual void SyncRendererPrefs() = 0;

  // Returns the current WebKit preferences. Note: WebPreferences is cached, so
  // this lookup will be fast
  virtual WebPreferences GetWebkitPreferences() = 0;

  // If any state that affects the webkit preferences changed, this method must
  // be called. This triggers recomputing preferences.
  virtual void OnWebkitPreferencesChanged() = 0;

  // Passes a list of Webkit preferences to the renderer.
  virtual void UpdateWebkitPreferences(const WebPreferences& prefs) = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewHostImpl;
  RenderViewHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
