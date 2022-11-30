// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/triggering_event_info.mojom-shared.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"

class GURL;

namespace blink {
namespace scheduler {
class WebAgentGroupScheduler;
}  // namespace scheduler
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class AssociatedInterfaceProvider;
class AssociatedInterfaceRegistry;
class BrowserInterfaceBrokerProxy;
class WebElement;
class WebFrame;
class WebLocalFrame;
class WebPlugin;
struct WebPluginParams;
class WebView;
}  // namespace blink

namespace gfx {
class Range;
class Rect;
class RectF;
}  // namespace gfx

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class RenderAccessibility;
struct RenderFrameMediaPlaybackOptions;
class RenderFrameVisitor;
struct WebPluginInfo;

// A class that takes a snapshot of the accessibility tree. Accessibility
// support in Blink is enabled for the lifetime of this object, which can
// be useful if you need consistent IDs between multiple snapshots.
class AXTreeSnapshotter {
 public:
  AXTreeSnapshotter() = default;

  // Return in |accessibility_tree| a snapshot of the accessibility tree
  // for the frame with the given accessibility mode.
  //
  // - |exclude_offscreen| excludes a subtree if a node is entirely offscreen,
  //   but note that this heuristic is imperfect, and an aboslute-positioned
  //   node that's visible, but whose ancestors are entirely offscreen, may
  //   get excluded.
  // - |max_nodes_count| specifies the maximum number of nodes to snapshot
  //   before exiting early. Note that this is not a hard limit; once this limit
  //   is reached a few more nodes may be added in order to ensure a
  //   well-formed tree is returned. Use 0 for no max.
  // - |timeout| will stop generating the result after a certain timeout
  //   (per frame), specified in milliseconds. Like max_node_count, this is not
  //   a hard limit, and once this/ limit is reached a few more nodes may
  //   be added in order to ensure a well-formed tree. Use 0 for no timeout.
  virtual void Snapshot(bool exclude_offscreen,
                        size_t max_node_count,
                        base::TimeDelta timeout,
                        ui::AXTreeUpdate* accessibility_tree) = 0;

  virtual ~AXTreeSnapshotter() = default;
};

// This interface wraps functionality, which is specific to frames, such as
// navigation. It provides communication with a corresponding RenderFrameHost
// in the browser process.
class CONTENT_EXPORT RenderFrame : public IPC::Listener,
                                   public IPC::Sender,
                                   public base::SupportsUserData {
 public:
  // These numeric values are used in UMA logs; do not change them.
  enum PeripheralContentStatus {
    // Content is peripheral, and should be throttled, but is not tiny.
    CONTENT_STATUS_PERIPHERAL = 0,
    // Content is essential because it's same-origin with the top-level frame.
    CONTENT_STATUS_ESSENTIAL_SAME_ORIGIN = 1,
    // Content is essential even though it's cross-origin, because it's large.
    CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_BIG = 2,
    // Content is essential because there's large content from the same origin.
    CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_ALLOWLISTED = 3,
    // Content is tiny in size. These are usually blocked.
    CONTENT_STATUS_TINY = 4,
    // Deprecated, as now entirely obscured content is treated as tiny.
    DEPRECATED_CONTENT_STATUS_UNKNOWN_SIZE = 5,
    // Must be last.
    CONTENT_STATUS_NUM_ITEMS
  };

  enum RecordPeripheralDecision {
    DONT_RECORD_DECISION = 0,
    RECORD_DECISION = 1
  };

  // Returns the RenderFrame given a WebLocalFrame.
  static RenderFrame* FromWebFrame(blink::WebLocalFrame* web_frame);

  // Returns the RenderFrame given a routing id.
  static RenderFrame* FromRoutingID(int routing_id);

  // Visit all live RenderFrames.
  static void ForEach(RenderFrameVisitor* visitor);

  // Returns the RenderFrame associated with the main frame of the WebView.
  // See `blink::WebView::MainFrame()`. Note that this will be null when
  // the main frame in this process is a remote frame.
  virtual RenderFrame* GetMainRenderFrame() = 0;

  // Return the RenderAccessibility associated with this frame.
  virtual RenderAccessibility* GetRenderAccessibility() = 0;

  // Return an object that can take a snapshot of the accessibility tree.
  // |ax_mode| is the accessibility mode to use, which determines which
  // fields of AXNodeData are populated when you make a snapshot.
  virtual std::unique_ptr<AXTreeSnapshotter> CreateAXTreeSnapshotter(
      ui::AXMode ax_mode) = 0;

  // Get the routing ID of the frame.
  virtual int GetRoutingID() = 0;

  // Returns the associated WebView.
  virtual blink::WebView* GetWebView() = 0;
  virtual const blink::WebView* GetWebView() const = 0;

  // Returns the associated WebFrame.
  virtual blink::WebLocalFrame* GetWebFrame() = 0;
  virtual const blink::WebLocalFrame* GetWebFrame() const = 0;

  // Gets WebKit related preferences associated with this frame.
  virtual const blink::web_pref::WebPreferences& GetBlinkPreferences() = 0;

  // Issues a request to show the virtual keyboard.
  virtual void ShowVirtualKeyboard() = 0;

  // Create a new Pepper plugin depending on |info|. Returns NULL if no plugin
  // was found.
  virtual blink::WebPlugin* CreatePlugin(
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) = 0;

  // Execute a string of JavaScript in this frame's context.
  virtual void ExecuteJavaScript(const std::u16string& javascript) = 0;

  // Returns true if this is the main (top-level) frame.
  virtual bool IsMainFrame() = 0;

  // Returns false if fenced frames are disabled. Returns true if the
  // feature is enabled and if |this| or any of its ancestor nodes is a
  // fenced frame.
  virtual bool IsInFencedFrameTree() const = 0;

  // Return true if this frame is hidden.
  virtual bool IsHidden() = 0;

  // Ask the RenderFrame (or its observers) to bind a request for
  // |interface_name| to |interface_pipe|.
  virtual void BindLocalInterface(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) = 0;

  // Returns the BrowserInterfaceBrokerProxy that this process can use to bind
  // interfaces exposed to it by the application running in this frame.
  virtual blink::BrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker() = 0;

  // Returns the AssociatedInterfaceRegistry this frame can use to expose
  // frame-specific Channel-associated interfaces to the remote RenderFrameHost.
  virtual blink::AssociatedInterfaceRegistry*
  GetAssociatedInterfaceRegistry() = 0;

  // Returns the AssociatedInterfaceProvider this frame can use to access
  // frame-specific Channel-associated interfaces from the remote
  // RenderFrameHost.
  virtual blink::AssociatedInterfaceProvider*
  GetRemoteAssociatedInterfaces() = 0;

  // Notifies the browser of text selection changes made.
  virtual void SetSelectedText(const std::u16string& selection_text,
                               size_t offset,
                               const gfx::Range& range) = 0;

  // Adds |message| to the DevTools console.
  virtual void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                                   const std::string& message) = 0;

  // Whether or not this frame is currently pasting.
  virtual bool IsPasting() = 0;

  // Loads specified |html| to this frame. |base_url| is used to resolve
  // relative urls in the document.
  // |replace_current_item| should be true if we load html instead of the
  // existing page. In this case |unreachable_url| might be the original url
  // which did fail loading.
  //
  // This should be used only for testing. Real code should follow the
  // navigation code path and inherit the correct security properties
  virtual void LoadHTMLStringForTesting(const std::string& html,
                                        const GURL& base_url,
                                        const std::string& text_encoding,
                                        const GURL& unreachable_url,
                                        bool replace_current_item) = 0;

  // Returns true in between the time that Blink requests navigation until the
  // browser responds with the result.
  // TODO(ahemery): Rename this to be more explicit.
  virtual bool IsBrowserSideNavigationPending() = 0;

  // Renderer scheduler frame-specific task queues handles.
  // See third_party/WebKit/Source/platform/WebFrameScheduler.h for details.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) = 0;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  virtual int GetEnabledBindings() = 0;

  // Set the accessibility mode to force creation of RenderAccessibility.
  virtual void SetAccessibilityModeForTest(ui::AXMode new_mode) = 0;

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Per-frame media playback options passed to each WebMediaPlayer.
  virtual const RenderFrameMediaPlaybackOptions&
  GetRenderFrameMediaPlaybackOptions() = 0;
  virtual void SetRenderFrameMediaPlaybackOptions(
      const RenderFrameMediaPlaybackOptions& opts) = 0;

  // Sets that cross browsing instance frame lookup is allowed.
  virtual void SetAllowsCrossBrowsingInstanceFrameLookup() = 0;

  // Returns the bounds of |element| in Window coordinates which are device
  // scale independent. The bounds have been adjusted to include any
  // transformations, including page scale. This function will update the layout
  // if required.
  virtual gfx::RectF ElementBoundsInWindow(
      const blink::WebElement& element) = 0;

  // Converts the |rect| to Window coordinates which are device scale
  // independent.
  virtual void ConvertViewportToWindow(gfx::Rect* rect) = 0;

  // Returns the device scale factor of the display the render frame is in.
  virtual float GetDeviceScaleFactor() = 0;

  // Return the dedicated scheduler for the AgentSchedulingGroup associated with
  // this RenderFrame.
  virtual blink::scheduler::WebAgentGroupScheduler&
  GetAgentGroupScheduler() = 0;

 protected:
  ~RenderFrame() override {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameImpl;
  RenderFrame() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
