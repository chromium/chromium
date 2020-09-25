// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. Any
// messages about the guest render process that the embedder might be interested
// in receiving should be listened for here.
//
// BrowserPluginGuest is a WebContentsObserver for the guest WebContents.
// BrowserPluginGuest operates under the assumption that the guest will be
// accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/web/web_drag_status.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

namespace content {
class RenderWidgetHostViewBase;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest-specific overrides for ViewHostMsg_*
// messages.
//
// When a guest is initially created, it is in an unattached state. That is,
// it is not visible anywhere and has no embedder WebContents assigned.
// A BrowserPluginGuest is said to be "attached" if it has an embedder.
// A BrowserPluginGuest can also create a new unattached guest via
// CreateNewWindow. The newly created guest will live in the same partition,
// which means it can share storage and can script this guest.
//
// Note: in --site-per-process, all IPCs sent out from this class will be
// dropped on the floor since we don't have a BrowserPlugin.
// TODO(wjmaclean): Get rid of "BrowserPlugin" in the name of this class.
// Perhaps "InnerWebContentsGuestConnector"?
class CONTENT_EXPORT BrowserPluginGuest : public GuestHost,
                                          public WebContentsObserver {
 public:
  ~BrowserPluginGuest() override;

  // The WebContents passed into the factory method here has not been
  // initialized yet and so it does not yet hold a SiteInstance.
  // BrowserPluginGuest must be constructed and installed into a WebContents
  // prior to its initialization because WebContents needs to determine what
  // type of WebContentsView to construct on initialization. The content
  // embedder needs to be aware of |guest_site_instance| on the guest's
  // construction and so we pass it in here.
  //
  // After this, a new BrowserPluginGuest is created with ownership transferred
  // into the |web_contents|.
  static void CreateInWebContents(WebContentsImpl* web_contents,
                                  BrowserPluginGuestDelegate* delegate);

  // Returns whether the given WebContents is a BrowserPlugin guest.
  static bool IsGuest(WebContentsImpl* web_contents);

  // BrowserPluginGuest::Init is called after the associated guest WebContents
  // initializes. If this guest cannot navigate without being attached to a
  // container, then this call is a no-op. For guest types that can be
  // navigated, this call adds the associated RenderWdigetHostViewGuest to the
  // view hierarchy and sets up the appropriate
  // blink::mojom::RendererPreferences so that this guest can navigate and
  // resize offscreen.
  void Init();

  // Returns a WeakPtr to this BrowserPluginGuest.
  base::WeakPtr<BrowserPluginGuest> AsWeakPtr();

  // Creates a new guest WebContentsImpl with the provided |params| with |this|
  // as the |opener|.
  WebContentsImpl* CreateNewGuestWindow(
      const WebContents::CreateParams& params);

  // WebContentsObserver implementation.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void RenderProcessGone(base::TerminationStatus status) override;
#if defined(OS_MAC)
  // On MacOS X popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
  bool ShowPopupMenu(
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr>* menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
#endif

  // GuestHost implementation.
  void WillDestroy() override;

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContentsImpl* GetWebContents() const;

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  void DragSourceEndedAt(float client_x,
                         float client_y,
                         float screen_x,
                         float screen_y,
                         blink::DragOperation operation);

  // Called when the drag started by this guest ends at an OS-level.
  void EmbedderSystemDragEnded();
  void EndSystemDragIfApplicable();

 protected:
  // BrowserPluginGuest is a WebContentsObserver of |web_contents| and
  // |web_contents| has to stay valid for the lifetime of BrowserPluginGuest.
  // Constructor protected for testing.
  BrowserPluginGuest(WebContentsImpl* web_contents,
                     BrowserPluginGuestDelegate* delegate);

 private:
  void InitInternal(WebContentsImpl* owner_web_contents);

  // Sets the focus state of the current RenderWidgetHostView.
  void SetFocus(bool focused, blink::mojom::FocusType focus_type);

  void SendTextInputTypeChangedToView(RenderWidgetHostViewBase* guest_rwhv);

  WebContentsImpl* owner_web_contents_;

  // BrowserPluginGuest::Init can only be called once. This flag allows it to
  // exit early if it's already been called.
  bool initialized_;

  // Text input type states.
  // Using scoped_ptr to avoid including the header file: view_messages.h.
  ui::mojom::TextInputStatePtr last_text_input_state_;

  // Last seen state of drag status update.
  blink::WebDragStatus last_drag_status_;
  // Whether or not our embedder has seen a SystemDragEnded() call.
  bool seen_embedder_system_drag_ended_;
  // Whether or not our embedder has seen a DragSourceEndedAt() call.
  bool seen_embedder_drag_source_ended_at_;

  BrowserPluginGuestDelegate* const delegate_;

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
