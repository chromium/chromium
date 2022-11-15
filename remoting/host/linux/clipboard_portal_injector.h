// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_INJECTOR_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_INJECTOR_H_

#include <gio/gio.h>

#include <unordered_set>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace remoting::xdg_portal {

// This class is used by the `ChromotingInputThread` to inject input into the
// wayland remote host using XDG desktop portal APIs.
class ClipboardPortalInjector {
 public:
  using ClipboardChangedCallbackSignature = void(const std::string& mime_type,
                                                 const std::string& data);
  using ClipboardChangedCallback =
      base::RepeatingCallback<ClipboardChangedCallbackSignature>;

  explicit ClipboardPortalInjector(
      ClipboardChangedCallback clipboard_changed_callback);
  ClipboardPortalInjector(const ClipboardPortalInjector&) = delete;
  ClipboardPortalInjector& operator=(const ClipboardPortalInjector&) = delete;
  ~ClipboardPortalInjector();

  // This method populates the session details for this object. Session details
  // are borrowed from the clipboard portal running under the wayland capturer
  void SetSessionDetails(
      const webrtc::xdg_portal::SessionDetails& session_details);

  // SetSelection starts the process of pasting content. It prompts the portal
  // backend to send a 'SelectionTransfer' signal, to which we will reply with a
  // 'SelectionWrite' and 'SelectionWriteDone'
  void SetSelection(std::string mime_type, std::string data);

 private:
  void SelectionRead(std::string mime_type);
  void SelectionWrite();
  void SelectionWriteDone(gboolean success);
  void SubscribeClipboardSignals();
  void UnsubscribeSignalHandlers();

  static void OnSelectionReadCallback(GDBusProxy* proxy,
                                      GAsyncResult* result,
                                      gpointer user_data);
  static void OnSelectionWriteCallback(GDBusProxy* proxy,
                                       GAsyncResult* result,
                                       gpointer user_data);
  static void OnSetSelectionCallback(GDBusProxy* proxy,
                                     GAsyncResult* result,
                                     gpointer user_data);
  static void OnSelectionWriteDoneCallback(GDBusProxy* proxy,
                                           GAsyncResult* result,
                                           gpointer user_data);
  static void OnSelectionTransferSignal(GDBusConnection* connection,
                                        const char* sender_name,
                                        const char* object_path,
                                        const char* interface_name,
                                        const char* signal_name,
                                        GVariant* parameters,
                                        gpointer user_data);
  static void OnSelectionOwnerChangedSignal(GDBusConnection* connection,
                                            const char* sender_name,
                                            const char* object_path,
                                            const char* interface_name,
                                            const char* signal_name,
                                            GVariant* parameters,
                                            gpointer user_data);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<GDBusConnection> connection_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<GDBusProxy> proxy_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<GCancellable> cancellable_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  std::string session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);

  ClipboardChangedCallback clipboard_changed_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unordered_set<std::string> writable_mime_type_set_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::string write_data_ GUARDED_BY_CONTEXT(sequence_checker_);
  guint write_serial_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unordered_set<std::string> readable_mime_type_set_
      GUARDED_BY_CONTEXT(sequence_checker_);

  guint selection_owner_changed_signal_id_ = 0;
  guint selection_transfer_signal_id_ = 0;
};

}  // namespace remoting::xdg_portal

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_INJECTOR_H_
