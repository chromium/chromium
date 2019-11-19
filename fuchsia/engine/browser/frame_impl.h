// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/zx/channel.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/discarding_event_filter.h"
#include "fuchsia/engine/browser/navigation_controller_impl.h"
#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"
#include "fuchsia/engine/on_load_script_injector.mojom.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

class ContextImpl;

// Implementation of fuchsia.web.Frame based on content::WebContents.
class FrameImpl : public fuchsia::web::Frame,
                  public content::WebContentsObserver,
                  public content::WebContentsDelegate {
 public:
  FrameImpl(std::unique_ptr<content::WebContents> web_contents,
            ContextImpl* context,
            fidl::InterfaceRequest<fuchsia::web::Frame> frame_request);
  ~FrameImpl() override;

  zx::unowned_channel GetBindingChannelForTest() const;
  content::WebContents* web_contents_for_test() const {
    return web_contents_.get();
  }
  bool has_view_for_test() const { return window_tree_host_ != nullptr; }
  void set_javascript_console_message_hook_for_test(
      base::RepeatingCallback<void(base::StringPiece)> hook) {
    console_log_message_hook_ = std::move(hook);
  }
  AccessibilityBridge* accessibility_bridge_for_test() const {
    return accessibility_bridge_.get();
  }
  void set_semantics_manager_for_test(
      fuchsia::accessibility::semantics::SemanticsManagerPtr
          semantics_manager) {
    test_semantics_manager_ptr_ = std::move(semantics_manager);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, DelayedNavigationEventAck);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NavigationObserverDisconnected);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NoNavigationObserverAttached);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, ReloadFrame);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, Stop);

  class OriginScopedScript {
   public:
    OriginScopedScript();
    OriginScopedScript(std::vector<std::string> origins,
                       base::ReadOnlySharedMemoryRegion script);
    OriginScopedScript& operator=(OriginScopedScript&& other);
    ~OriginScopedScript();

    const std::vector<std::string>& origins() const { return origins_; }
    const base::ReadOnlySharedMemoryRegion& script() const { return script_; }

   private:
    std::vector<std::string> origins_;

    // A shared memory buffer containing the script, encoded as UTF16.
    base::ReadOnlySharedMemoryRegion script_;

    DISALLOW_COPY_AND_ASSIGN(OriginScopedScript);
  };

  aura::Window* root_window() const { return window_tree_host_->window(); }

  // Release the resources associated with the View, if one is active.
  void TearDownView();

  // Shared implementation for the ExecuteJavaScript[NoResult]() APIs.
  void ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                 fuchsia::mem::Buffer script,
                                 ExecuteJavaScriptCallback callback,
                                 bool need_result);

  // Sends the next entry in |pending_popups_| to |popup_listener_|.
  void MaybeSendPopup();

  void OnPopupListenerDisconnected(zx_status_t status);

  // fuchsia::web::Frame implementation.
  void CreateView(fuchsia::ui::views::ViewToken view_token) override;
  void GetNavigationController(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller)
      override;
  void ExecuteJavaScript(std::vector<std::string> origins,
                         fuchsia::mem::Buffer script,
                         ExecuteJavaScriptCallback callback) override;
  void ExecuteJavaScriptNoResult(
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      ExecuteJavaScriptNoResultCallback callback) override;
  void AddBeforeLoadJavaScript(
      uint64_t id,
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      AddBeforeLoadJavaScriptCallback callback) override;
  void RemoveBeforeLoadJavaScript(uint64_t id) override;
  void PostMessage(std::string origin,
                   fuchsia::web::WebMessage message,
                   fuchsia::web::Frame::PostMessageCallback callback) override;
  void SetNavigationEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener)
      override;
  void SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) override;
  void SetEnableInput(bool enable_input) override;
  void SetPopupFrameCreationListener(
      fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener)
      override;
  void SetUrlRequestRewriteRules(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
      SetUrlRequestRewriteRulesCallback callback) override;

  // content::WebContentsDelegate implementation.
  void CloseContents(content::WebContents* source) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

  // content::WebContentsObserver implementation.
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  std::unique_ptr<aura::WindowTreeHost> window_tree_host_;
  const std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<wm::FocusController> focus_controller_;
  ContextImpl* const context_;
  std::unique_ptr<AccessibilityBridge> accessibility_bridge_;
  fuchsia::accessibility::semantics::SemanticsManagerPtr
      test_semantics_manager_ptr_;

  DiscardingEventFilter discarding_event_filter_;
  NavigationControllerImpl navigation_controller_;
  logging::LogSeverity log_level_;
  std::map<uint64_t, OriginScopedScript> before_load_scripts_;
  std::vector<uint64_t> before_load_scripts_order_;
  base::RepeatingCallback<void(base::StringPiece)> console_log_message_hook_;
  UrlRequestRewriteRulesManager url_request_rewrite_rules_manager_;

  // Used for receiving and dispatching popup created by this Frame.
  fuchsia::web::PopupFrameCreationListenerPtr popup_listener_;
  std::list<std::unique_ptr<content::WebContents>> pending_popups_;
  bool popup_ack_outstanding_ = false;

  fidl::Binding<fuchsia::web::Frame> binding_;

  DISALLOW_COPY_AND_ASSIGN(FrameImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
