// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_gnome.h"

#include <array>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

namespace {

constexpr char kRemoteDesktopBusName[] = "org.gnome.Mutter.RemoteDesktop";

// This list was created by looking at the MIME types claimed by some Wayland
// apps that put text onto the clipboard. The first few are claimed by X11 apps
// using XWayland (such as xclip).
constexpr std::array<std::string_view, 5> kTextMimeTypes = {
    "UTF8_STRING", "STRING", "TEXT", "text/plain;charset=utf-8", "text/plain"};

}  // namespace

ClipboardGnome::ClipboardGnome(GDBusConnectionRef connection,
                               gvariant::ObjectPath session_path)
    : dbus_connection_(std::move(connection)),
      session_path_(std::move(session_path)) {}

ClipboardGnome::~ClipboardGnome() = default;

void ClipboardGnome::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_.swap(client_clipboard);

  selection_owner_changed_signal_ = dbus_connection_.SignalSubscribe<
      org_gnome_Mutter_RemoteDesktop_Session::SelectionOwnerChanged>(
      kRemoteDesktopBusName, session_path_,
      base::BindRepeating(&ClipboardGnome::OnSelectionOwnerChanged,
                          weak_factory_.GetWeakPtr()));
  selection_transfer_signal_ = dbus_connection_.SignalSubscribe<
      org_gnome_Mutter_RemoteDesktop_Session::SelectionTransfer>(
      kRemoteDesktopBusName, session_path_,
      base::BindRepeating(&ClipboardGnome::OnSelectionTransfer,
                          weak_factory_.GetWeakPtr()));

  dbus_connection_
      .Call<org_gnome_Mutter_RemoteDesktop_Session::EnableClipboard>(
          kRemoteDesktopBusName, session_path_,
          std::tuple(gvariant::EmptyArrayOf<"{sv}">()),
          base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "EnableClipboard failed: " << result.error();
            }
          }));
}

void ClipboardGnome::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  if (event.mime_type() != kMimeTypeTextUtf8) {
    LOG(ERROR) << "Non UTF-8 clipboard type from client: " << event.mime_type();
    return;
  }

  clipboard_data_ = std::move(event.data());

  // Claim ownership of the text mime-types. After this, GNOME may request the
  // clipboard data via one or more SelectionTransfer signals.

  // The variant type here is inconsistent with what GNOME returns in the
  // SelectionOwnerChanged signal. Here it must be "as", not "(as)":
  // https://gitlab.gnome.org/GNOME/gnome-remote-desktop/-/blob/979631ffeac75f79fad104c3e479dd7513cc2478/src/grd-session.c#L804
  auto mime_types_option = GVariantFrom(gvariant::BoxedRef(kTextMimeTypes));
  dbus_connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::SetSelection>(
      kRemoteDesktopBusName, session_path_,
      std::tuple(std::array{std::pair{"mime-types", mime_types_option}}),
      base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "SetSelection failed: " << result.error();
        }
      }));
}

void ClipboardGnome::OnSelectionOwnerChanged(
    std::tuple<GVariantRef<"a{sv}">> args) {
  HOST_LOG << "Received SelectionOwnerChanged signal.";

  auto& [options] = args;

  auto maybe_boxed_is_owner = options.LookUp("session-is-owner");
  if (maybe_boxed_is_owner.has_value()) {
    // "session-is-owner" is a variant holding a boolean.
    if (*maybe_boxed_is_owner == GVariantFrom(gvariant::Boxed{true})) {
      // The selection is now owned by this D-Bus caller (a recent SetSelection
      // call succeeded). SelectionRead should only be used to request the data
      // from a different owner.
      HOST_LOG << "Ignoring event, session is already owner.";
      return;
    }
  }

  auto maybe_boxed_mime_types = options.LookUp("mime-types");
  if (maybe_boxed_mime_types.has_value()) {
    std::vector<std::string> mime_types;
    // std::tie is needed here because mime-types has type "(as)", not "as".
    auto destructure_result =
        maybe_boxed_mime_types->TryDestructure(std::tie(mime_types));
    if (destructure_result.has_value()) {
      for (auto mime_type : mime_types) {
        if (base::Contains(kTextMimeTypes, mime_type)) {
          SelectionRead(mime_type);
          return;
        }
      }
    }
  }
}

void ClipboardGnome::OnSelectionTransfer(
    std::tuple<std::string, std::uint32_t> args) {
  const auto& [mime_type, serial] = args;
  HOST_LOG << "Got SelectionTransfer signal with mime-type: " << mime_type;

  if (!base::Contains(kTextMimeTypes, mime_type)) {
    // SelectionTransfer request should be for a mime-type claimed by
    // SetSelection.
    LOG(ERROR) << "Unexpected mime-type requested: " << mime_type;
    return;
  }

  SelectionWrite(serial);
}

void ClipboardGnome::SelectionRead(std::string_view mime_type) {
  dbus_connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::SelectionRead>(
      kRemoteDesktopBusName, session_path_, std::tuple(mime_type),
      base::BindOnce(&ClipboardGnome::OnSelectionReadReply,
                     weak_factory_.GetWeakPtr()));
}

void ClipboardGnome::OnSelectionReadReply(
    base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                   Loggable> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "SelectionRead failed: " << result.error();
    return;
  }

  auto& [tuple, dbus_fds] = result.value();
  auto& [read_handle] = tuple;
  SparseFdList fd_list = std::move(dbus_fds).MakeSparse();
  base::ScopedFD read_fd = fd_list.Extract(read_handle);
  if (!read_fd.is_valid()) {
    LOG(ERROR) << "Failed to get FD for SelectionRead.";
    return;
  }

  fd_string_reader_ = FdStringReader::ReadFromPipe(
      std::move(read_fd), base::BindOnce(&ClipboardGnome::OnSelectionDataRead,
                                         weak_factory_.GetWeakPtr()));
}

void ClipboardGnome::OnSelectionDataRead(
    base::expected<std::string, Loggable> result) {
  // Close the FD.
  fd_string_reader_.reset();
  if (result.has_value()) {
    HOST_LOG << "Got clipboard data of size: " << result.value().size();
    protocol::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(std::move(result.value()));
    client_clipboard_->InjectClipboardEvent(event);
  } else {
    LOG(ERROR) << "Failed to read clipboard data: " << result.error();
  }
}

void ClipboardGnome::SelectionWrite(std::uint32_t serial) {
  selection_write_serial_ = serial;
  dbus_connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::SelectionWrite>(
      kRemoteDesktopBusName, session_path_, std::tuple(serial),
      base::BindOnce(&ClipboardGnome::OnSelectionWriteReply,
                     weak_factory_.GetWeakPtr()));
}

void ClipboardGnome::OnSelectionWriteReply(
    base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                   Loggable> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "SelectionWrite failed: " << result.error();
    return;
  }

  auto& [tuple, dbus_fds] = result.value();
  auto& [write_handle] = tuple;
  SparseFdList fd_list = std::move(dbus_fds).MakeSparse();
  base::ScopedFD write_fd = fd_list.Extract(write_handle);
  if (!write_fd.is_valid()) {
    LOG(ERROR) << "Failed to get FD for SelectionWrite.";
    return;
  }

  fd_string_writer_ = FdStringWriter::Write(
      clipboard_data_, std::move(write_fd),
      base::BindOnce(&ClipboardGnome::OnSelectionDataWritten,
                     weak_factory_.GetWeakPtr()));
}

void ClipboardGnome::OnSelectionDataWritten(
    base::expected<void, Loggable> result) {
  // Close the FD.
  fd_string_writer_.reset();
  bool success = result.has_value();
  if (!success) {
    LOG(ERROR) << "Failed to write clipboard data: " << result.error();
  }

  dbus_connection_
      .Call<org_gnome_Mutter_RemoteDesktop_Session::SelectionWriteDone>(
          kRemoteDesktopBusName, session_path_,
          std::tuple(selection_write_serial_, success),
          base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "SelectionWriteDone returned error: "
                         << result.error();
            }
          }));
}

}  // namespace remoting
