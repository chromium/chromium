// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_zoom.h"
#include "ipc/ipc_sender.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"

namespace perfetto::protos::pbzero {
class RenderViewHost;
}  // namespace perfetto::protos::pbzero

namespace content {

class RenderProcessHost;
class RenderWidgetHost;

// A RenderViewHost is responsible for creating and talking to a
// `blink::WebView` object in a child process. It exposes a high level API to
// users, for things like loading pages, adjusting the display and other browser
// functionality, which it translates into IPC messages sent over the IPC
// channel with the `blink::WebView`. It responds to all IPC messages sent by
// that `blink::WebView` and cracks them, calling a delegate object back with
// higher level types where possible.
//
// The intent of this interface is to provide a view-agnostic communication
// conduit with a renderer. This is so we can build HTML views not only as
// WebContents (see WebContents for an example) but also as views, etc.
//
// DEPRECATED: RenderViewHost is being removed as part of the SiteIsolation
// project. New code should not be added here, but to RenderWidgetHost (if it's
// about drawing or events), RenderFrameHost (if it's frame specific), or
// Page (if it's page specific).
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHost {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // Returns the RenderViewHost, if any, that uses the specified
  // RenderWidgetHost. Returns nullptr if there is no such RenderViewHost.
  static RenderViewHost* From(RenderWidgetHost* rwh);

  virtual ~RenderViewHost() {}

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

  // Instructs the `blink::WebView` to send back updates to the preferred size.
  virtual void EnablePreferredSizeMode() = 0;

  using TraceProto = perfetto::protos::pbzero::RenderViewHost;
  // Write a representation of this object into a trace.
  virtual void WriteIntoTrace(
      perfetto::TracedProto<TraceProto> context) const = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewHostImpl;
  RenderViewHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
