// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_GNOME_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_GNOME_H_

#include <cstdint>
#include <string>
#include <tuple>

#include "base/memory/weak_ptr.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/linux/fd_string_reader.h"
#include "remoting/host/linux/fd_string_writer.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// Clipboard implementation that uses the clipboard APIs from GNOME's
// D-Bus RemoteDesktop interface. These GNOME APIs only support a single
// "clipboard" selection - the middle-click "primary" selection is not
// supported.
class ClipboardGnome : public Clipboard {
 public:
  ClipboardGnome(GDBusConnectionRef connection,
                 gvariant::ObjectPath session_path);
  ClipboardGnome(const ClipboardGnome&) = delete;
  ClipboardGnome& operator=(const ClipboardGnome&) = delete;
  ~ClipboardGnome() override;

  // Clipboard interface
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  // D-Bus signal handlers
  void OnSelectionOwnerChanged(std::tuple<GVariantRef<"a{sv}">> args);
  void OnSelectionTransfer(std::tuple<std::string, std::uint32_t> args);

  void SelectionRead(std::string_view mime_type);
  void OnSelectionReadReply(
      base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                     Loggable>);
  void OnSelectionDataRead(base::expected<std::string, Loggable> result);

  void SelectionWrite(std::uint32_t serial);
  void OnSelectionWriteReply(
      base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                     Loggable> result);
  void OnSelectionDataWritten(base::expected<void, Loggable>);

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;

  // Needed for making D-Bus calls on the RemoteDesktop session.
  GDBusConnectionRef dbus_connection_;
  gvariant::ObjectPath session_path_;

  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      selection_owner_changed_signal_;
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      selection_transfer_signal_;

  std::unique_ptr<FdStringReader> fd_string_reader_;
  std::unique_ptr<FdStringWriter> fd_string_writer_;

  // Holds the latest clipboard data injected by the client, owned by this
  // process. It is stored so that other apps can read it when the user pastes
  // the selection into some other app.
  std::string clipboard_data_;

  // Serial number for the current SelectionWrite process if any.
  std::uint32_t selection_write_serial_;

  base::WeakPtrFactory<ClipboardGnome> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_GNOME_H_
