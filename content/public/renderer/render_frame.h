// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/bindings_policy.h"
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
class WebFrame;
class WebLocalFrame;
class WebPlugin;
struct WebPluginParams;
class WebView;
}  // namespace blink

namespace gfx {
class Range;
class Rect;
}  // namespace gfx

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
  // - |max_nodes_count| specifies the maximum number of nodes to snapshot
  //   before exiting early. Note that this is not a hard limit; once this limit
  //   is reached a few more nodes may be added in order to ensure a
  //   well-formed tree is returned. Use 0 for no max.
  // - |timeout| will stop generating the result after a certain timeout
  //   (per frame), specified in milliseconds. Like max_node_count, this is not
  //   a hard limit, and once this/ limit is reached a few more nodes may
  //   be added in order to ensure a well-formed tree. Use 0 for no timeout.
  virtual void Snapshot(size_t max_node_count,
                        base::TimeDelta timeout,
                        ui::AXTreeUpdate* accessibility_tree) = 0;

  virtual ~AXTreeSnapshotter() = default;
};

// This interface wraps functionality, which is specific to frames, such as
// navigation. It provides communication with a corresponding RenderFrameHost
// in the browser process.
class CONTENT_EXPORT RenderFrame :
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    public IPC::Listener,
    public IPC::Sender,
#endif
    public base::SupportsUserData {
 public:
  // Returns the RenderFrame given a WebLocalFrame.
  static RenderFrame* FromWebFrame(blink::WebLocalFrame* web_frame);

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Returns the RenderFrame given a routing id.
  static RenderFrame* FromRoutingID(int routing_id);
#endif

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

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Get the routing ID of the frame.
  virtual int GetRoutingID() = 0;
#endif

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
  virtual const blink::BrowserInterfaceBrokerProxy&
  GetBrowserInterfaceBroker() = 0;

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
  virtual void LoadHTMLStringForTesting(std::string_view html,
                                        const GURL& base_url,
                                        const std::string& text_encoding,
                                        const GURL& unreachable_url,
                                        bool replace_current_item) = 0;

  // Returns true in between the time that Blink requests navigation until the
  // browser responds with the result.
  virtual bool IsRequestingNavigation() = 0;

  // Renderer scheduler frame-specific task queues handles.
  // See third_party/WebKit/Source/platform/WebFrameScheduler.h for details.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) = 0;

  // The extra bindings that have been enabled.
  virtual BindingsPolicySet GetEnabledBindings() = 0;

  // Set the accessibility mode to force creation of RenderAccessibility.
  virtual void SetAccessibilityModeForTest(ui::AXMode new_mode) = 0;

  // Per-frame media playback options passed to each WebMediaPlayer.
  virtual const RenderFrameMediaPlaybackOptions&
  GetRenderFrameMediaPlaybackOptions() = 0;
  virtual void SetRenderFrameMediaPlaybackOptions(
      const RenderFrameMediaPlaybackOptions& opts) = 0;

  // Sets that cross browsing instance frame lookup is allowed.
  virtual void SetAllowsCrossBrowsingInstanceFrameLookup() = 0;

  // Converts the |rect| to Window coordinates which are device scale
  // independent. The bounds have been adjusted to include any transformations,
  // including page scale.
  [[nodiscard]] virtual gfx::Rect ConvertViewportToWindow(
      const gfx::Rect& rect) = 0;

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
