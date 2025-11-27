// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_CAPTURE_STREAM_MANAGER_H_
#define REMOTING_HOST_LINUX_PORTAL_CAPTURE_STREAM_MANAGER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class PipewireCaptureStream;
class ScopedPortalRequest;

// This class is used to create and manage a list of Pipewire capture streams
// that are created through the XDG desktop portal.
// The current implementation will always create one virtual stream after Init()
// is called. It is not possible to add or remove virtual streams afterwards.
class PortalCaptureStreamManager final : public CaptureStreamManager {
 public:
  using InitCallbackSignature = void(base::expected<void, std::string>);
  using InitCallback = base::OnceCallback<InitCallbackSignature>;

  PortalCaptureStreamManager();
  ~PortalCaptureStreamManager() override;

  PortalCaptureStreamManager(const PortalCaptureStreamManager&) = delete;
  PortalCaptureStreamManager& operator=(const PortalCaptureStreamManager&) =
      delete;

  // Initializes the stream manager. Must be called once before calling other
  // methods of this class.
  //
  // `remote_desktop_session_handle` is a session handle that has been created
  // with the `org.freedesktop.portal.RemoteDesktop.CreateSession` method. This
  // method will select the VIRTUAL source and call Start on the specified
  // session handle.
  void Init(GDBusConnectionRef connection,
            gvariant::ObjectPath remote_desktop_session_handle,
            InitCallback callback);

  // CaptureStreamManager interface.
  Observer::Subscription AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer);
  base::WeakPtr<CaptureStream> GetStream(webrtc::ScreenId screen_id) override;
  void AddVirtualStream(const ScreenResolution& initial_resolution,
                        AddStreamCallback callback) override;
  void RemoveVirtualStream(webrtc::ScreenId screen_id) override;
  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
  GetActiveStreams() override;

  base::WeakPtr<PortalCaptureStreamManager> GetWeakPtr();

 private:
  struct PendingStream {
    uint32_t pipewire_node_id;
    std::string mapping_id;
  };

  // Calls `success_method` if the returned callback is called with a result,
  // otherwise calls OnInitError().
  // Resets `request` iff it is not a nullptr (i.e. pointing to a unique_ptr).
  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckInitResultAndContinue(
      void (PortalCaptureStreamManager::*success_method)(SuccessType),
      std::unique_ptr<ScopedPortalRequest>* request,
      String&& error_context);

  // No-op if the returned callback is called with a result, otherwise resets
  // `request` and calls OnInitError().
  template <typename String>
  GDBusConnectionRef::CallCallback<GVariantRef<"(o)">> ResetRequestOnFailure(
      std::unique_ptr<ScopedPortalRequest>& request,
      String&& error_context);

  void OnInitError(std::string_view what, Loggable why);

  // DBus callback handlers:
  void OnSelectSourcesResponse(gvariant::GVariantRef<"a{sv}"> result);
  void OnStartResponse(gvariant::GVariantRef<"a{sv}"> result);
  void OnPipeWireStreamOpened(
      std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> result);

  std::vector<PendingStream> pending_streams_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // We are using the PipeWire node ID as the screen ID.
  base::flat_map<webrtc::ScreenId, std::unique_ptr<PipewireCaptureStream>>
      streams_ GUARDED_BY_CONTEXT(sequence_checker_);

  InitCallback init_callback_;
  gvariant::ObjectPath remote_desktop_session_handle_;
  GDBusConnectionRef connection_;
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ScopedFD pipewire_fd_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<ScopedPortalRequest> select_sources_request_;
  std::unique_ptr<ScopedPortalRequest> start_request_;

  base::WeakPtrFactory<PortalCaptureStreamManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_CAPTURE_STREAM_MANAGER_H_
