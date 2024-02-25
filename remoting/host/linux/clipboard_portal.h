// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_

#include <gio/gio.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/webrtc/modules/portal/portal_request_response.h"
#include "third_party/webrtc/modules/portal/xdg_session_details.h"

namespace remoting::xdg_portal {

// Helper class to setup an XDG clipboard portal. It uses the same session
// handle as the remote desktop and screencast portals (hence they should have
// the same lifetime.) The methods on this class are called from the capturer
// thread.
class ClipboardPortal {
 public:
  // |notifier| must outlive |ClipboardPortal| instance and will be called
  // into from the capturer thread.
  class PortalNotifier {
   public:
    // Called when the clipboard portal has been instantiated.
    virtual void OnClipboardPortalDone(
        webrtc::xdg_portal::RequestResponse result) = 0;

   protected:
    PortalNotifier() = default;
    virtual ~PortalNotifier() = default;
  };

  explicit ClipboardPortal(PortalNotifier* notifier);

  ClipboardPortal(const ClipboardPortal&) = delete;
  ClipboardPortal& operator=(const ClipboardPortal&) = delete;
  ~ClipboardPortal();

  // Starts the portal setup.
  void Start();

  // Sets details about the remote desktop session being used.
  void SetSessionDetails(
      const webrtc::xdg_portal::SessionDetails& session_details);
  webrtc::xdg_portal::SessionDetails GetSessionDetails();

  // Sends a request for clipboard access to portal.  Must be called after the
  // session details have been set and the proxy requested.
  void RequestClipboard();

 private:
  void OnPortalDone(webrtc::xdg_portal::RequestResponse result);

  static void OnClipboardRequest(GObject* object,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnClipboardPortalProxyRequested(GObject* object,
                                              GAsyncResult* result,
                                              gpointer user_data);

  raw_ptr<GDBusConnection> connection_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<GDBusProxy> proxy_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<GCancellable> cancellable_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  std::string session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<PortalNotifier> notifier_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingCallback<webrtc::xdg_portal::SessionDetails()>
      clipboard_session_details_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting::xdg_portal

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
