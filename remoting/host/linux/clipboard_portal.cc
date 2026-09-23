// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_portal.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/base/constants.h"
#include "remoting/base/loggable.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_Clipboard.h"
#include "remoting/host/linux/gdbus_fd_list.h"
#include "remoting/host/linux/gvariant_dict_builder.h"
#include "remoting/host/linux/portal_utils.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

class ClipboardPortal::ClientProxy : public Clipboard {
 public:
  explicit ClientProxy(base::WeakPtr<ClipboardPortal> clipboard_portal)
      : clipboard_portal_(std::move(clipboard_portal)) {}
  ClientProxy(const ClientProxy&) = delete;
  ClientProxy& operator=(const ClientProxy&) = delete;
  ~ClientProxy() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    client_clipboard_ = std::move(client_clipboard);
    if (clipboard_portal_) {
      clipboard_portal_->SetClientProxy(weak_factory_.GetWeakPtr());
    }
  }

  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (clipboard_portal_) {
      clipboard_portal_->InjectClipboardEvent(event);
    }
  }

  void DeliverHostClipboardEvent(const protocol::ClipboardEvent& event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (client_clipboard_) {
      client_clipboard_->InjectClipboardEvent(event);
    }
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtr<ClipboardPortal> clipboard_portal_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<protocol::ClipboardStub> client_clipboard_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<ClientProxy> weak_factory_{this};
};

// static
std::unique_ptr<Clipboard> ClipboardPortal::CreateClientProxy(
    ClipboardPortal* clipboard_portal) {
  return std::make_unique<ClientProxy>(
      clipboard_portal ? clipboard_portal->GetWeakPtr() : nullptr);
}

ClipboardPortal::ClipboardPortal(GDBusConnectionRef connection,
                                 gvariant::ObjectPath session_handle)
    : dbus_connection_(std::move(connection)),
      session_handle_(std::move(session_handle)) {
  selection_owner_changed_signal_ = dbus_connection_.SignalSubscribe<
      org_freedesktop_portal_Clipboard::SelectionOwnerChanged>(
      kPortalBusName, kPortalObjectPath,
      base::BindRepeating(&ClipboardPortal::OnSelectionOwnerChanged,
                          weak_factory_.GetWeakPtr()));
  selection_transfer_signal_ =
      dbus_connection_
          .SignalSubscribe<org_freedesktop_portal_Clipboard::SelectionTransfer>(
              kPortalBusName, kPortalObjectPath,
              base::BindRepeating(&ClipboardPortal::OnSelectionTransfer,
                                  weak_factory_.GetWeakPtr()));
}

ClipboardPortal::~ClipboardPortal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::uint32_t> pending_serials;
  pending_serials.reserve(fd_string_writers_.size());
  for (const auto& [serial, _] : fd_string_writers_) {
    pending_serials.push_back(serial);
  }
  for (std::uint32_t serial : pending_serials) {
    SelectionWriteDone(serial, /*success=*/false);
  }
}

void ClipboardPortal::SetClientProxy(base::WeakPtr<ClientProxy> client_proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_proxy_ = std::move(client_proxy);
  if (client_proxy_ && host_clipboard_data_.has_value()) {
    protocol::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(*host_clipboard_data_);
    client_proxy_->DeliverHostClipboardEvent(event);
  }
}

void ClipboardPortal::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event.mime_type() != kMimeTypeTextUtf8) {
    LOG(ERROR) << "Non UTF-8 clipboard type from client: " << event.mime_type();
    return;
  }

  CancelPendingRead();
  clipboard_data_ = event.data();
  host_clipboard_data_ = event.data();

  // Claim ownership of the text mime-types. After this, the portal may request
  // the clipboard data via one or more SelectionTransfer signals.
  dbus_connection_.Call<org_freedesktop_portal_Clipboard::SetSelection>(
      kPortalBusName, kPortalObjectPath,
      std::tuple(
          session_handle_,
          GVariantDictBuilder().Add("mime_types", kTextMimeTypes).Build()),
      base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "SetSelection failed: " << result.error();
        }
      }));
}

base::WeakPtr<ClipboardPortal> ClipboardPortal::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ClipboardPortal::OnSelectionOwnerChanged(
    std::tuple<gvariant::ObjectPath, gvariant::GVariantRef<"a{sv}">> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& [session_handle, options] = args;
  if (session_handle != session_handle_) {
    return;
  }

  HOST_LOG << "Received SelectionOwnerChanged signal.";

  if (ReadGVariantDictValue<bool>(options, "session_is_owner")
          .value_or(false)) {
    // The selection is now owned by this D-Bus caller (a recent SetSelection
    // call succeeded). SelectionRead should only be used to request the data
    // from a different owner.
    HOST_LOG << "Ignoring event, session is already owner.";
    return;
  }

  CancelPendingRead();

  auto maybe_boxed_mime_types = options.LookUp("mime_types");
  if (!maybe_boxed_mime_types.has_value()) {
    return;
  }

  std::vector<std::string> mime_types;
  auto destructure_result =
      maybe_boxed_mime_types
          ->TryInto<gvariant::Boxed<std::vector<std::string>>>();
  if (destructure_result.has_value()) {
    mime_types = std::move(destructure_result->value);
  } else {
    // Fallback in case a backend wraps the string array in a tuple "(as)".
    auto tuple_result =
        maybe_boxed_mime_types->TryDestructure(std::tie(mime_types));
    if (!tuple_result.has_value()) {
      LOG(ERROR) << "Failed to read mime_types from SelectionOwnerChanged: "
                 << tuple_result.error();
      return;
    }
  }

  for (auto mime_type : kTextMimeTypes) {
    if (std::ranges::contains(mime_types, mime_type)) {
      SelectionRead(mime_type);
      return;
    }
  }
}

void ClipboardPortal::OnSelectionTransfer(
    std::tuple<gvariant::ObjectPath, std::string, std::uint32_t> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& [session_handle, mime_type, serial] = args;
  if (session_handle != session_handle_) {
    return;
  }

  HOST_LOG << "Got SelectionTransfer signal with mime-type: " << mime_type;

  if (fd_string_writers_.contains(serial)) {
    LOG(ERROR) << "Transfer already in progress for serial: " << serial;
    return;
  }

  if (!std::ranges::contains(kTextMimeTypes, mime_type)) {
    // SelectionTransfer request should be for a mime-type claimed by
    // SetSelection.
    LOG(ERROR) << "Unexpected mime-type requested: " << mime_type;
    SelectionWriteDone(serial, /*success=*/false);
    return;
  }

  SelectionWrite(serial);
}

void ClipboardPortal::CancelPendingRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_clipboard_data_.reset();
  fd_string_reader_.reset();
  read_weak_factory_.InvalidateWeakPtrs();
}

void ClipboardPortal::SelectionRead(std::string_view mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dbus_connection_.Call<org_freedesktop_portal_Clipboard::SelectionRead>(
      kPortalBusName, kPortalObjectPath, std::tuple(session_handle_, mime_type),
      base::BindOnce(&ClipboardPortal::OnSelectionReadReply,
                     read_weak_factory_.GetWeakPtr()));
}

void ClipboardPortal::OnSelectionReadReply(
    base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                   Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      std::move(read_fd), base::BindOnce(&ClipboardPortal::OnSelectionDataRead,
                                         read_weak_factory_.GetWeakPtr()));
}

void ClipboardPortal::OnSelectionDataRead(
    base::expected<std::string, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Close the FD.
  fd_string_reader_.reset();
  if (result.has_value()) {
    HOST_LOG << "Got clipboard data of size: " << result.value().size();
    host_clipboard_data_ = std::move(result.value());
    if (client_proxy_) {
      protocol::ClipboardEvent event;
      event.set_mime_type(kMimeTypeTextUtf8);
      event.set_data(*host_clipboard_data_);
      client_proxy_->DeliverHostClipboardEvent(event);
    }
  } else {
    LOG(ERROR) << "Failed to read clipboard data: " << result.error();
  }
}

void ClipboardPortal::SelectionWrite(std::uint32_t serial) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fd_string_writers_.emplace(serial, nullptr);
  dbus_connection_.Call<org_freedesktop_portal_Clipboard::SelectionWrite>(
      kPortalBusName, kPortalObjectPath, std::tuple(session_handle_, serial),
      base::BindOnce(&ClipboardPortal::OnSelectionWriteReply,
                     weak_factory_.GetWeakPtr(), serial));
}

void ClipboardPortal::OnSelectionWriteReply(
    std::uint32_t serial,
    base::expected<std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList>,
                   Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    LOG(ERROR) << "SelectionWrite failed: " << result.error();
    SelectionWriteDone(serial, /*success=*/false);
    return;
  }

  auto& [tuple, dbus_fds] = result.value();
  auto& [write_handle] = tuple;
  SparseFdList fd_list = std::move(dbus_fds).MakeSparse();
  base::ScopedFD write_fd = fd_list.Extract(write_handle);
  if (!write_fd.is_valid()) {
    LOG(ERROR) << "Failed to get FD for SelectionWrite.";
    SelectionWriteDone(serial, /*success=*/false);
    return;
  }

  fd_string_writers_[serial] = FdStringWriter::Write(
      clipboard_data_, std::move(write_fd),
      base::BindOnce(&ClipboardPortal::OnSelectionDataWritten,
                     weak_factory_.GetWeakPtr(), serial));
}

void ClipboardPortal::OnSelectionDataWritten(
    std::uint32_t serial,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool success = result.has_value();
  if (!success) {
    LOG(ERROR) << "Failed to write clipboard data: " << result.error();
  }

  SelectionWriteDone(serial, success);
}

void ClipboardPortal::SelectionWriteDone(std::uint32_t serial, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Close the FD (if any) before notifying the portal via SelectionWriteDone.
  fd_string_writers_.erase(serial);
  dbus_connection_.Call<org_freedesktop_portal_Clipboard::SelectionWriteDone>(
      kPortalBusName, kPortalObjectPath,
      std::tuple(session_handle_, serial, success),
      base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "SelectionWriteDone returned error: " << result.error();
        }
      }));
}

}  // namespace remoting
