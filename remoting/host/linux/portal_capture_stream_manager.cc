// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_capture_stream_manager.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_RemoteDesktop.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_ScreenCast.h"
#include "remoting/host/linux/gvariant_dict_builder.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/host/linux/portal_utils.h"
#include "remoting/host/linux/scoped_portal_request.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

namespace {

using gvariant::Boxed;
using gvariant::GVariantRef;

}  // namespace

PortalCaptureStreamManager::StreamInfo::StreamInfo() = default;
PortalCaptureStreamManager::StreamInfo::StreamInfo(StreamInfo&&) = default;
PortalCaptureStreamManager::StreamInfo::~StreamInfo() = default;
PortalCaptureStreamManager::StreamInfo&
PortalCaptureStreamManager::StreamInfo::operator=(StreamInfo&&) = default;

PortalCaptureStreamManager::PortalCaptureStreamManager() = default;
PortalCaptureStreamManager::~PortalCaptureStreamManager() = default;

void PortalCaptureStreamManager::Init(
    bool create_virtual_monitor,
    GDBusConnectionRef connection,
    gvariant::ObjectPath remote_desktop_session_handle,
    InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_ = connection;
  remote_desktop_session_handle_ = std::move(remote_desktop_session_handle);
  init_callback_ = std::move(callback);

  select_sources_request_ = std::make_unique<ScopedPortalRequest>(
      connection_,
      CheckInitResultAndContinue(
          &PortalCaptureStreamManager::OnSelectSourcesResponse,
          &select_sources_request_, "ScreenCast.SelectSources failed"));

  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html#org-freedesktop-portal-screencast-availablesourcetypes
  uint32_t types = create_virtual_monitor ? /*VIRTUAL*/ 4u : /*MONITOR*/ 1u;

  GVariantRef<"a{sv}"> options =
      GVariantDictBuilder()
          .Add("handle_token", select_sources_request_->token())
          .Add("multiple", true)
          .Add("types", types)
          // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html#org-freedesktop-portal-screencast-availablecursormodes
          .Add("cursor_mode", /*METADATA*/ 4u)
          .Build();

  connection_.Call<org_freedesktop_portal_ScreenCast::SelectSources>(
      kPortalBusName, kPortalObjectPath,
      std::make_tuple(remote_desktop_session_handle_, options),
      ResetRequestOnFailure(select_sources_request_,
                            "ScreenCast.SelectSources failed"));
}

CaptureStreamManager::Observer::Subscription
PortalCaptureStreamManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
  return base::ScopedClosureRunner(
      base::BindOnce(&PortalCaptureStreamManager::RemoveObserver,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

void PortalCaptureStreamManager::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

base::WeakPtr<CaptureStream> PortalCaptureStreamManager::GetStream(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = streams_.find(screen_id);
  if (it == streams_.end()) {
    return nullptr;
  }
  return it->second.stream->GetWeakPtr();
}

void PortalCaptureStreamManager::AddVirtualStream(
    const ScreenResolution& initial_resolution,
    AddStreamCallback callback) {
  NOTREACHED() << "Adding virtual stream is not supported.";
}

void PortalCaptureStreamManager::RemoveVirtualStream(
    webrtc::ScreenId screen_id) {
  NOTREACHED() << "Removing virtual stream is not supported.";
}

base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
PortalCaptureStreamManager::GetActiveStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>> result;
  for (auto const& [screen_id, stream_info] : streams_) {
    result[screen_id] = stream_info.stream->GetWeakPtr();
  }
  return result;
}

base::flat_map<webrtc::ScreenId, const webrtc::DesktopRect*>
PortalCaptureStreamManager::GetActiveStreamInitialRects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_map<webrtc::ScreenId, const webrtc::DesktopRect*> result;
  for (auto const& [screen_id, stream_info] : streams_) {
    result[screen_id] = &stream_info.initial_rect;
  }
  return result;
}

base::WeakPtr<PortalCaptureStreamManager>
PortalCaptureStreamManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

template <typename SuccessType, typename String>
GDBusConnectionRef::CallCallback<SuccessType>
PortalCaptureStreamManager::CheckInitResultAndContinue(
    void (PortalCaptureStreamManager::*success_method)(SuccessType),
    std::unique_ptr<ScopedPortalRequest>* request,
    String&& error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<PortalCaptureStreamManager> that,
         decltype(success_method) success_method,
         std::unique_ptr<ScopedPortalRequest>* request,
         std::string_view error_context,
         base::expected<SuccessType, Loggable> result) {
        if (!that) {
          return;
        }
        if (request) {
          request->reset();
        }
        if (result.has_value()) {
          (that.get()->*success_method)(std::move(result).value());
        } else {
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), success_method, request,
      std::forward<String>(error_context));
}

template <typename String>
GDBusConnectionRef::CallCallback<GVariantRef<"(o)">>
PortalCaptureStreamManager::ResetRequestOnFailure(
    std::unique_ptr<ScopedPortalRequest>& request,
    String&& error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<PortalCaptureStreamManager> that,
         std::unique_ptr<ScopedPortalRequest>& request,
         const std::string& error_context,
         base::expected<GVariantRef<"(o)">, Loggable> result) {
        if (!that) {
          return;
        }
        if (!result.has_value()) {
          request.reset();
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::ref(request),
      std::forward<String>(error_context));
}

void PortalCaptureStreamManager::OnInitError(std::string_view error_message,
                                             Loggable error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_callback_) {
    std::move(init_callback_)
        .Run(base::unexpected(
            base::StrCat({error_message, ": ", error_context.ToString()})));
  }
}

void PortalCaptureStreamManager::OnSelectSourcesResponse(
    GVariantRef<"a{sv}"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "ScreenCast.SelectSources succeeded.";

  start_request_ = std::make_unique<ScopedPortalRequest>(
      connection_, CheckInitResultAndContinue(
                       &PortalCaptureStreamManager::OnStartResponse,
                       &start_request_, "RemoteDesktop.Start failed"));

  GVariantRef<"a{sv}"> options_ref =
      GVariantDictBuilder()
          .Add("handle_token", start_request_->token())
          .Build();

  connection_.Call<org_freedesktop_portal_RemoteDesktop::Start>(
      kPortalBusName, kPortalObjectPath,
      std::make_tuple(remote_desktop_session_handle_,
                      /*parent_window=*/std::string_view{}, options_ref),
      ResetRequestOnFailure(start_request_, "RemoteDesktop.Start failed"));
}

void PortalCaptureStreamManager::OnStartResponse(GVariantRef<"a{sv}"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "RemoteDesktop.Start succeeded.";

  // Array of {pipewire_node_id, options}. See
  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html#org-freedesktop-portal-screencast-start
  auto streams_variant_opt = result.LookUp("streams");
  if (!streams_variant_opt) {
    LOG(ERROR) << "No \"streams\" key in RemoteDesktop.Start response.";
    return;
  }

  auto streams_variant_boxed =
      streams_variant_opt->TryInto<Boxed<GVariantRef<"a(ua{sv})">>>();
  if (!streams_variant_boxed.has_value()) {
    LOG(ERROR) << "Failed to convert streams variant: "
               << streams_variant_boxed.error();
    return;
  }
  auto streams_variant = streams_variant_boxed->value;
  for (const auto& stream_variant : streams_variant) {
    uint32_t pipewire_node_id;
    GVariantRef<"a{sv}"> options;
    stream_variant.Destructure(pipewire_node_id, options);
    std::string mapping_id;
    auto mapping_id_expected =
        ReadGVariantDictValue<std::string>(options, "mapping_id");
    if (mapping_id_expected.has_value()) {
      mapping_id = std::move(*mapping_id_expected);
    } else {
      // Some DEs do not support mapping IDs, in which case we will just use an
      // empty string.
      HOST_LOG << "No mapping id found for stream " << pipewire_node_id << ": "
               << mapping_id_expected.error();
    }
    // KDE can currently only negotiate stream format at 1920x1080:
    // https://bugs.kde.org/show_bug.cgi?id=512620
    static const webrtc::DesktopSize kDefaultResolution =
        webrtc::DesktopSize(1920, 1080);
    webrtc::DesktopRect initial_rect;
    auto position_expected =
        ReadGVariantDictValue<std::tuple<int, int>>(options, "position");
    if (position_expected.has_value()) {
      initial_rect = webrtc::DesktopRect::MakeOriginSize(
          std::make_from_tuple<webrtc::DesktopVector>(*position_expected),
          kDefaultResolution);
    } else {
      LOG(WARNING) << "No position found for stream " << pipewire_node_id
                   << ": " << position_expected.error();
      initial_rect = webrtc::DesktopRect::MakeSize(kDefaultResolution);
    }
    auto size_expected =
        ReadGVariantDictValue<std::tuple<int, int>>(options, "size");
    if (size_expected.has_value()) {
      initial_rect = webrtc::DesktopRect::MakeOriginSize(
          initial_rect.top_left(),
          std::make_from_tuple<webrtc::DesktopSize>(*size_expected));
    } else {
      LOG(WARNING) << "No size found for stream " << pipewire_node_id << ": "
                   << size_expected.error();
    }
    pending_streams_.emplace_back(pipewire_node_id, mapping_id, initial_rect);
  }

  if (pending_streams_.empty()) {
    OnInitError("RemoteDesktop.Start response error",
                Loggable(FROM_HERE, "Response contains no streams."));
    return;
  }

  connection_.Call<org_freedesktop_portal_ScreenCast::OpenPipeWireRemote>(
      kPortalBusName, kPortalObjectPath,
      std::make_tuple(remote_desktop_session_handle_,
                      gvariant::EmptyArrayOf<"{sv}">()),
      CheckInitResultAndContinue(
          &PortalCaptureStreamManager::OnPipeWireStreamOpened,
          /*request=*/nullptr, "ScreenCast.OpenPipeWireRemote failed"));
}

void PortalCaptureStreamManager::OnPipeWireStreamOpened(
    std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto fd_list = std::move(result.second).MakeSparse();
  auto [handle] = result.first;
  pipewire_fd_ = fd_list.Extract(handle);
  if (!pipewire_fd_.is_valid()) {
    OnInitError("Failed to get PipeWire stream FD",
                Loggable(FROM_HERE, "Handle not present in FD list"));
    return;
  }

  HOST_LOG << "PipeWire remote FD opened: " << pipewire_fd_.get();

  for (PendingStream& pending_stream : pending_streams_) {
    auto stream = std::make_unique<PipewireCaptureStream>();
    webrtc::ScreenId screen_id = pending_stream.pipewire_node_id;
    stream->SetPipeWireStream(pending_stream.pipewire_node_id,
                              /*initial_resolution=*/{},
                              pending_stream.mapping_id, pipewire_fd_.get());
    stream->set_screen_id(screen_id);
    stream->StartVideoCapture();

    auto weak_ptr = stream->GetWeakPtr();
    auto& stream_info = streams_[screen_id];
    stream_info.stream = std::move(stream);
    stream_info.initial_rect = pending_stream.initial_rect;
    observers_.Notify(&Observer::OnPipewireCaptureStreamAdded, weak_ptr);
    HOST_LOG << "Stream " << screen_id << " added.";
  }

  pending_streams_.clear();

  if (init_callback_) {
    std::move(init_callback_).Run(base::ok());
  }
}

}  // namespace remoting
