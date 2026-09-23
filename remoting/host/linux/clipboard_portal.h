// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/linux/fd_string_reader.h"
#include "remoting/host/linux/fd_string_writer.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gdbus_fd_list.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// Manages clipboard synchronization over the org.freedesktop.portal.Clipboard
// D-Bus interface for the lifetime of a PortalRemoteDesktopSession.
// Only the standard "clipboard" selection is supported by the portal API; the
// middle-click "primary" selection is not supported.
class ClipboardPortal {
 public:
  // Creates a per-client Clipboard wrapper forwarding to `clipboard_portal`.
  // `clipboard_portal` may be null if portal clipboard support is unavailable,
  // in which case the returned wrapper is a no-op.
  static std::unique_ptr<Clipboard> CreateClientProxy(
      ClipboardPortal* clipboard_portal);

  ClipboardPortal(GDBusConnectionRef connection,
                  gvariant::ObjectPath session_handle);
  ClipboardPortal(const ClipboardPortal&) = delete;
  ClipboardPortal& operator=(const ClipboardPortal&) = delete;
  ~ClipboardPortal();

  void InjectClipboardEvent(const protocol::ClipboardEvent& event);

 private:
  class ClientProxy;

  base::WeakPtr<ClipboardPortal> GetWeakPtr();
  void SetClientProxy(base::WeakPtr<ClientProxy> client_proxy);

  // D-Bus signal handlers.
  void OnSelectionOwnerChanged(
      std::tuple<gvariant::ObjectPath, gvariant::GVariantRef<"a{sv}">> args);
  void OnSelectionTransfer(
      std::tuple<gvariant::ObjectPath, std::string, std::uint32_t> args);

  void CancelPendingRead();
  void SelectionRead(std::string_view mime_type);
  void OnSelectionReadReply(
      base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                     Loggable> result);
  void OnSelectionDataRead(base::expected<std::string, Loggable> result);

  void SelectionWrite(std::uint32_t serial);
  void OnSelectionWriteReply(
      std::uint32_t serial,
      base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                     Loggable> result);
  void OnSelectionDataWritten(std::uint32_t serial,
                              base::expected<void, Loggable> result);
  void SelectionWriteDone(std::uint32_t serial, bool success);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<ClientProxy> client_proxy_
      GUARDED_BY_CONTEXT(sequence_checker_);

  GDBusConnectionRef dbus_connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      selection_owner_changed_signal_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      selection_transfer_signal_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<FdStringReader> fd_string_reader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<std::uint32_t, std::unique_ptr<FdStringWriter>>
      fd_string_writers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the latest known UTF-8 text on the host clipboard (either read from a
  // local app or injected by a client), so a newly connecting client receives
  // the current clipboard state immediately upon connecting.
  std::optional<std::string> host_clipboard_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the latest clipboard data injected by the client, owned by this
  // process. It is stored so that local apps can continue reading it via
  // SelectionTransfer even after the client disconnects.
  std::string clipboard_data_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Separate WeakPtrFactory used to cancel in-flight SelectionRead callbacks
  // when a newer clipboard owner takes over.
  base::WeakPtrFactory<ClipboardPortal> read_weak_factory_{this};
  base::WeakPtrFactory<ClipboardPortal> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
