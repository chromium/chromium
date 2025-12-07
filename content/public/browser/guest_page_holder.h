// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GUEST_PAGE_HOLDER_H_
#define CONTENT_PUBLIC_BROWSER_GUEST_PAGE_HOLDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_stream_request.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blink {
struct RendererPreferences;
}  // namespace blink

namespace gfx {
class Size;
}

namespace content {

struct ContextMenuParams;
struct OpenURLParams;
class NavigationController;
class NavigationHandle;
class JavaScriptDialogManager;
class RenderFrameHost;
class SiteInstance;
class WebContents;

// Hosts the contents of a guest page. Guests are kinds of embedded pages, but
// their semantics are mostly delegated outside of the content/ layer. See
// components/guest_view/README.md.
class GuestPageHolder : public base::SupportsUserData {
 public:
  // Used to notify guest implementations about events within the guest and to
  // have the delegate provide necessary functionality.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Analogous to `WebContentsObserver::DidStopLoading`.
    virtual void GuestDidStopLoading() = 0;

    // Asks the delegate to open/show the context menu based on `params`.
    //
    // The `render_frame_host` represents the frame that requests the context
    // menu (typically this frame is focused, but this is not necessarily the
    // case - see https://crbug.com/1257907#c14).
    //
    // Returns true if the context menu operation was handled by the delegate.
    virtual bool GuestHandleContextMenu(RenderFrameHost& render_frame_host,
                                        const ContextMenuParams& params) = 0;

    // Returns a pointer to a service to manage JavaScript dialogs. May return
    // nullptr in which case dialogs aren't shown.
    virtual JavaScriptDialogManager* GuestGetJavascriptDialogManager() = 0;

    // Allow the delegate to mutate the RendererPreferences.
    virtual void GuestOverrideRendererPreferences(
        blink::RendererPreferences& preferences) = 0;

    // The main frame was completely loaded.
    virtual void GuestDocumentOnLoadCompleted() = 0;

    // The main frame's loading progress changed.
    virtual void GuestDidChangeLoadProgress(double progress) = 0;

    // The main frame's process is gone.
    virtual void GuestMainFrameProcessGone(base::TerminationStatus status) = 0;

    // The guest was resized.
    virtual void GuestResizeDueToAutoResize(const gfx::Size& new_size) = 0;

    // The guest's preferred size changed.
    virtual void GuestUpdateWindowPreferredSize(const gfx::Size& pref_size) = 0;

    // Return the prospective outer document. Should only be called when
    // unattached.
    virtual RenderFrameHost* GetProspectiveOuterDocument() = 0;

    // Create a new window with the disposition and URL.
    virtual GuestPageHolder* GuestCreateNewWindow(
        WindowOpenDisposition disposition,
        const GURL& url,
        const std::string& main_frame_name,
        RenderFrameHost* opener,
        scoped_refptr<SiteInstance> site_instance) = 0;

    // Open an URL from the current GuestPageHolder.
    virtual void GuestOpenURL(const OpenURLParams& params,
                              base::OnceCallback<void(NavigationHandle&)>
                                  navigation_handle_callback) = 0;

    // Close the current window.
    virtual void GuestClose() = 0;

    // Asks permission to use the camera and/or microphone.
    // See `WebContentsDelegate::RequestMediaAccessPermission`
    virtual void GuestRequestMediaAccessPermission(
        const MediaStreamRequest& request,
        MediaResponseCallback callback) = 0;

    // Checks if we have permission to access the microphone or camera.
    // See `WebContentsDelegate::CheckMediaAccessPermission`
    virtual bool GuestCheckMediaAccessPermission(
        RenderFrameHost* render_frame_host,
        const url::Origin& security_origin,
        blink::mojom::MediaStreamType type) = 0;

    // TODO(40202416): Guest implementations need to be informed of several
    // other events that they currently get through primary main frame specific
    // WebContentsObserver methods (e.g.
    // DocumentOnLoadCompletedInPrimaryMainFrame).
  };

  // Creates a new guest page. The caller takes ownership until it is ready to
  // attach the guest to its embedder with `WebContents::AttachGuestPage`.
  CONTENT_EXPORT static std::unique_ptr<GuestPageHolder> Create(
      WebContents* owner_web_contents,
      scoped_refptr<SiteInstance> site_instance,
      base::WeakPtr<GuestPageHolder::Delegate> delegate);

  // Creates a new guest page with opener arguments.
  CONTENT_EXPORT static std::unique_ptr<GuestPageHolder> CreateWithOpener(
      WebContents* owner_web_contents,
      const std::string& frame_name,
      RenderFrameHost* opener,
      scoped_refptr<SiteInstance> site_instance,
      base::WeakPtr<GuestPageHolder::Delegate> delegate);

  ~GuestPageHolder() override = default;

  // Returns the NavigationController of the guest frame tree.
  virtual NavigationController& GetController() = 0;

  // Returns the current main frame of the guest page.
  virtual RenderFrameHost* GetGuestMainFrame() = 0;

  // Indicates/Sets whether all audio output from this guest page is muted.
  // This does not affect audio capture, just local/system output.
  virtual bool IsAudioMuted() = 0;
  virtual void SetAudioMuted(bool mute) = 0;

  virtual RenderFrameHost* GetOpener() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class GuestPageHolderImpl;
  GuestPageHolder() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GUEST_PAGE_HOLDER_H_
