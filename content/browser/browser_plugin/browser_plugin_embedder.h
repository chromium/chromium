// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginEmbedder handles messages coming from a BrowserPlugin's
// embedder that are not directed at any particular existing guest process.
// In the beginning, when a BrowserPlugin instance in the embedder renderer
// process requests an initial navigation, the WebContents for that renderer
// renderer creates a BrowserPluginEmbedder for itself. The
// BrowserPluginEmbedder, in turn, forwards the requests to a
// BrowserPluginGuestManager, which creates and manages the lifetime of the new
// guest.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/platform/web_drag_operation.h"

struct BrowserPluginHostMsg_Attach_Params;

namespace content {

class BrowserPluginGuest;
class BrowserPluginGuestManager;
struct NativeWebKeyboardEvent;

class CONTENT_EXPORT BrowserPluginEmbedder : public WebContentsObserver {
 public:
  ~BrowserPluginEmbedder() override;

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents);

  // Called when embedder's |rwh| has sent screen rects to renderer.
  void DidSendScreenRects();

  // WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  // Sends a 'dragend' message to the guest that started the drag.
  void DragSourceEndedAt(float client_x,
                         float client_y,
                         float screen_x,
                         float screen_y,
                         blink::WebDragOperation operation);

  // Indicates that a drag operation has entered into the bounds of a given
  // |guest|. Returns whether the |guest| also started the operation.
  bool DragEnteredGuest(BrowserPluginGuest* guest);

  // Indicates that a drag operation has left the bounds of a given |guest|.
  void DragLeftGuest(BrowserPluginGuest* guest);

  // Closes modal dialogs in all of the guests.
  void CancelGuestDialogs();

  // Called by WebContentsViewGuest when a drag operation is started within
  // |guest|. This |guest| will be signaled at the end of the drag operation.
  void StartDrag(BrowserPluginGuest* guest);

  // Sends EndSystemDrag message to the guest that initiated the last drag/drop
  // operation, if there's any.
  void SystemDragEnded();

  // The page wants to update the mouse cursor during a drag & drop
  // operation. This update will be suppressed if the cursor is dragging over a
  // guest.
  bool OnUpdateDragCursor();

  // Used to handle special keyboard events.
  bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Returns the "full page" guest if there is one. That is, if there is a
  // single BrowserPlugin in the embedder which takes up the full page, then it
  // is returned.
  BrowserPluginGuest* GetFullPageGuest();

  // Polls all guests for this web contents and returns true if any of them
  // are currently audible.
  bool AreAnyGuestsCurrentlyAudible();

 private:
  explicit BrowserPluginEmbedder(WebContentsImpl* web_contents);

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  void ClearGuestDragStateIfApplicable();

  static bool DidSendScreenRectsCallback(WebContents* guest_web_contents);

  // Closes modal dialogs in |guest_web_contents|.
  static bool CancelDialogs(WebContents* guest_web_contents);

  static bool UnlockMouseIfNecessaryCallback(bool* mouse_unlocked,
                                             WebContents* guest);

  static bool GuestCurrentlyAudibleCallback(WebContents* guest);

  // Message handlers.

  void OnAttach(RenderFrameHost* render_frame_host,
                int instance_id,
                const BrowserPluginHostMsg_Attach_Params& params);

  // Used to correctly update the cursor when dragging over a guest, and to
  // handle a race condition when dropping onto the guest that started the drag
  // (the race is that the dragend message arrives before the drop message so
  // the drop never takes place).
  // crbug.com/233571
  base::WeakPtr<BrowserPluginGuest> guest_dragging_over_;

  // Pointer to the guest that started the drag, used to forward necessary drag
  // status messages to the correct guest.
  base::WeakPtr<BrowserPluginGuest> guest_started_drag_;

  // Keeps track of "dragend" state.
  bool guest_drag_ending_;

  base::WeakPtrFactory<BrowserPluginEmbedder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
