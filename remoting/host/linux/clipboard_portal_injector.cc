// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_portal_injector.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <stdio.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"

namespace remoting::xdg_portal {
namespace {

constexpr char kClipboardInterfaceName[] = "org.freedesktop.portal.Clipboard";
constexpr char kPortalMimeTypeTextUtf8[] = "text/plain;charset=utf-8";

using webrtc::Scoped;
using webrtc::xdg_portal::kDesktopBusName;
using webrtc::xdg_portal::kDesktopObjectPath;
using webrtc::xdg_portal::SessionDetails;

void UnsubscribeSignalHandler(GDBusConnection* connection, guint& signal_id) {
  if (signal_id) {
    g_dbus_connection_signal_unsubscribe(connection, signal_id);
    signal_id = 0;
  }
}

// portal expects 'text/plain;charset=utf-8' while we use 'text/plain;
// charset=UTF-8'
static std::string TranslateMimeTypeForPortal(std::string mime_type) {
  if (mime_type == kMimeTypeTextUtf8) {
    return kPortalMimeTypeTextUtf8;
  }

  return mime_type;
}

}  // namespace

ClipboardPortalInjector::ClipboardPortalInjector(
    ClipboardChangedCallback clipboard_changed_back)
    : clipboard_changed_callback_(clipboard_changed_back) {
  readable_mime_type_set_.insert(kPortalMimeTypeTextUtf8);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ClipboardPortalInjector::~ClipboardPortalInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnsubscribeSignalHandlers();
}

void ClipboardPortalInjector::UnsubscribeSignalHandlers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnsubscribeSignalHandler(connection_, selection_owner_changed_signal_id_);
  UnsubscribeSignalHandler(connection_, selection_transfer_signal_id_);
}

void ClipboardPortalInjector::SetSessionDetails(
    const webrtc::xdg_portal::SessionDetails& session_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Desktop portal session details received";
  if (session_details.proxy) {
    proxy_ = session_details.proxy;
    connection_ = g_dbus_proxy_get_connection(proxy_);
  }
  if (session_details.cancellable) {
    cancellable_ = session_details.cancellable;
  }
  if (!session_details.session_handle.empty()) {
    session_handle_ = session_details.session_handle;
  }

  SubscribeClipboardSignals();
}

void ClipboardPortalInjector::SetSelection(std::string mime_type,
                                           std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());

  // Currently only UTF-8 is supported (which is consistent with x11.)
  if (mime_type != kMimeTypeTextUtf8) {
    LOG(ERROR)
        << "ClipboardEvent: data should be UTF-8, but an unexpected mime "
           "type was received: "
        << mime_type;
    return;
  }

  if (!base::IsStringUTF8AllowingNoncharacters(data)) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded";
    return;
  }

  mime_type = TranslateMimeTypeForPortal(mime_type);
  write_data_ = data;

  if (!writable_mime_type_set_.contains(mime_type)) {
    writable_mime_type_set_.insert(mime_type);
  }

  GVariantBuilder options_builder;
  GVariantBuilder mime_types_string_builder;

  g_variant_builder_init(&options_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_init(&mime_types_string_builder,
                         G_VARIANT_TYPE_STRING_ARRAY);

  for (auto it : writable_mime_type_set_) {
    g_variant_builder_add(&mime_types_string_builder, "s", it.c_str());
  }

  g_variant_builder_add(&options_builder, "{sv}", "mime_types",
                        g_variant_builder_end(&mime_types_string_builder));

  g_dbus_proxy_call(
      proxy_, "SetSelection",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &options_builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      OnSetSelectionCallback, this);
}

// static
void ClipboardPortalInjector::OnSetSelectionCallback(GObject* object,
                                                     GAsyncResult* result,
                                                     gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    LOG(ERROR) << "Failed to set selection: " << error->message;
    return;
  }
}

void ClipboardPortalInjector::SelectionWrite(const uint serial) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(serial);
  DCHECK(!write_data_.empty());

  write_serial_ = serial;

  Scoped<GError> error;
  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "SelectionWrite",
      g_variant_new("(ou)", session_handle_.c_str(), write_serial_),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, nullptr, cancellable_,
      OnSelectionWriteCallback, this);
}

// static
void ClipboardPortalInjector::OnSelectionWriteCallback(GObject* object,
                                                       GAsyncResult* result,
                                                       gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  gboolean success;

  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));
  if (!variant) {
    LOG(ERROR) << "Failed to write selection: " << error->message;
    return;
  }

  int32_t fd_id;
  Scoped<GError> fd_error;
  success = false;

  fd_id = g_variant_get_handle(variant.get());

  base::ScopedFD fd(
      g_unix_fd_list_get(outlist.get(), fd_id, fd_error.receive()));

  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to get file descriptor from the list: "
               << fd_error->message;
  } else {
    success = base::WriteFileDescriptor(fd.get(), that->write_data_);
    if (!success) {
      LOG(ERROR) << "Failed to write clipboard data to file descriptor";
    }
  }

  that->SelectionWriteDone(that->write_serial_, success);
  that->write_serial_ = 0;
}

void ClipboardPortalInjector::SelectionWriteDone(const uint serial,
                                                 const gboolean success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());

  g_dbus_proxy_call(
      proxy_, "SelectionWriteDone",
      g_variant_new("(oub)", session_handle_.c_str(), serial, success),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      OnSelectionWriteDoneCallback, this);
}

// static
void ClipboardPortalInjector::OnSelectionWriteDoneCallback(GObject* object,
                                                           GAsyncResult* result,
                                                           gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    LOG(ERROR) << "Failed selection write done: " << error->message;
    return;
  }
}

void ClipboardPortalInjector::SelectionRead(std::string mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());

  // we can only process one `SelectionRead` at a time, so eat any extra
  // requests we receive. if any requests get eaten, then we perform another
  // read to capture the latest contents.
  if (pending_selection_read_) {
    queued_selection_read_ = true;
    return;
  }
  pending_selection_read_ = true;

  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "SelectionRead",
      g_variant_new("(os)", session_handle_.c_str(), mime_type.c_str()),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, nullptr, cancellable_,
      OnSelectionReadCallback, this);
}

// static
void ClipboardPortalInjector::OnSelectionReadCallback(GObject* object,
                                                      GAsyncResult* result,
                                                      gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  auto* that = static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;

  that->pending_selection_read_ = false;

  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));

  if (that->queued_selection_read_) {
    that->queued_selection_read_ = false;
    that->SelectionRead(TranslateMimeTypeForPortal(kMimeTypeTextUtf8));
    return;
  }

  if (!variant) {
    LOG(ERROR) << "Failed to read selection: " << error->message;
    return;
  }

  int32_t index;
  g_variant_get(variant.get(), "(h)", &index);

  base::ScopedFD fd(g_unix_fd_list_get(outlist.get(), index, error.receive()));

  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to get file descriptor from the list: "
               << error->message;
    return;
  }

  std::string read_data;
  base::ScopedFILE stream(fdopen(fd.release(), "rb"));
  if (!stream.get()) {
    return;
  }

  if (base::ReadStreamToString(stream.get(), &read_data)) {
    that->clipboard_changed_callback_.Run(kMimeTypeTextUtf8, read_data);
  }
}

void ClipboardPortalInjector::SubscribeClipboardSignals() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The 'SelectionOwnerChanged' signal is sent anytime the selection changes.
  // We listen for notifications on it to know when we should copy the
  // selection.
  selection_owner_changed_signal_id_ = g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kClipboardInterfaceName,
      "SelectionOwnerChanged", kDesktopObjectPath, nullptr,
      G_DBUS_SIGNAL_FLAGS_NONE,
      static_cast<GDBusSignalCallback>(OnSelectionOwnerChangedSignal), this,
      nullptr);

  // The 'SelectionTransfer' signal is sent to request for our selection data.
  // We use it whenever we want to update the current selection.
  selection_transfer_signal_id_ = g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kClipboardInterfaceName,
      "SelectionTransfer", kDesktopObjectPath, nullptr,
      G_DBUS_SIGNAL_FLAGS_NONE,
      static_cast<GDBusSignalCallback>(OnSelectionTransferSignal), this,
      nullptr);
}

// static
void ClipboardPortalInjector::OnSelectionTransferSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  ClipboardPortalInjector* that =
      static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  HOST_LOG << "Received transfer selection signal from session";

  guint serial;
  g_variant_get(parameters, "(osu)", /*session_handle*/ nullptr,
                /*mime_type*/ nullptr, &serial);

  if (!that->write_serial_) {
    that->SelectionWrite(serial);
  } else {
    // we only process one write request at a time, so tell the backend to
    // release any extra serials it allocates.
    LOG(ERROR)
        << "Received clipboard write serial while busy processing another.";
    that->SelectionWriteDone(serial, false);
  }
}

// static
void ClipboardPortalInjector::OnSelectionOwnerChangedSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  ClipboardPortalInjector* that =
      static_cast<ClipboardPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  HOST_LOG << "Received selection owner changed signal from session";

  uint32_t session_handle;
  Scoped<GVariant> options;

  g_variant_get(parameters, "(o@a{sv})", &session_handle, options.receive());

  Scoped<GVariant> session_is_owner(g_variant_lookup_value(
      options.get(), "session_is_owner", G_VARIANT_TYPE_BOOLEAN));
  if (session_is_owner && g_variant_get_boolean(session_is_owner.get())) {
    return;
  }

  Scoped<GVariant> mime_types(g_variant_lookup_value(
      options.get(), "mime_types", G_VARIANT_TYPE("(as)")));
  if (!mime_types) {
    return;
  }

  GVariantIter iterator;
  gchar* mime_type;

  g_variant_iter_init(&iterator,
                      g_variant_get_child_value(mime_types.get(), 0));

  while (g_variant_iter_loop(&iterator, "s", &mime_type)) {
    if (that->readable_mime_type_set_.contains(mime_type)) {
      that->SelectionRead(mime_type);
      g_free(mime_type);
      break;
    }
  }
}

}  // namespace remoting::xdg_portal
