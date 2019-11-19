// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_
#define EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/media_perception/media_perception.pb.h"
#include "chromeos/services/media_perception/public/mojom/media_perception_service.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/media_perception_private.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {

class MediaPerceptionAPIManager
    : public BrowserContextKeyedAPI,
      public chromeos::MediaAnalyticsClient::Observer {
 public:
  using APISetAnalyticsComponentCallback = base::OnceCallback<void(
      extensions::api::media_perception_private::ComponentState
          component_state)>;

  using APIComponentProcessStateCallback = base::OnceCallback<void(
      extensions::api::media_perception_private::ProcessState process_state)>;

  using APIStateCallback = base::OnceCallback<void(
      extensions::api::media_perception_private::State state)>;

  using APIGetDiagnosticsCallback = base::Callback<void(
      extensions::api::media_perception_private::Diagnostics diagnostics)>;

  explicit MediaPerceptionAPIManager(content::BrowserContext* context);
  ~MediaPerceptionAPIManager() override;

  // Convenience method to get the MediaPeceptionAPIManager for a
  // BrowserContext.
  static MediaPerceptionAPIManager* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>*
  GetFactoryInstance();

  // Handler for clients of the API requesting a MediaPerception Mojo interface.
  void ActivateMediaPerception(
      mojo::PendingReceiver<chromeos::media_perception::mojom::MediaPerception>
          receiver);

  // Public functions for MediaPerceptionPrivateAPI implementation.
  void SetAnalyticsComponent(
      const extensions::api::media_perception_private::Component& component,
      APISetAnalyticsComponentCallback callback);
  void SetComponentProcessState(
      const extensions::api::media_perception_private::ProcessState&
          process_state,
      APIComponentProcessStateCallback callback);
  void GetState(APIStateCallback callback);
  void SetState(const extensions::api::media_perception_private::State& state,
                APIStateCallback callback);
  void GetDiagnostics(const APIGetDiagnosticsCallback& callback);

  // For testing purposes only. Allows the unittest to set the mount_point to
  // something non-empty.
  void SetMountPointNonEmptyForTesting();

 private:
  friend class BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>;

  class MediaPerceptionControllerClient;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "MediaPerceptionAPIManager"; }

  enum class AnalyticsProcessState {
    // The process is not running.
    IDLE,
    // The process has been launched via Upstart, but waiting for callback to
    // confirm.
    CHANGING_PROCESS_STATE,
    // The process is running.
    RUNNING,
    // The process state is unknown, e.g. when a Upstart Stop request fails.
    UNKNOWN
  };

  // Sets the state of the analytics process.
  void SetStateInternal(APIStateCallback callback, const mri::State& state);

  // MediaAnalyticsClient::Observer overrides.
  void OnDetectionSignal(const mri::MediaPerception& media_perception) override;

  // Callback for State D-Bus method calls to the media analytics process.
  void StateCallback(APIStateCallback callback,
                     base::Optional<mri::State> state);

  // Callback for GetDiagnostics D-Bus method calls to the media analytics
  // process.
  void GetDiagnosticsCallback(const APIGetDiagnosticsCallback& callback,
                              base::Optional<mri::Diagnostics> diagnostics);

  // Callbacks for Upstart command to start media analytics process.
  void UpstartStartProcessCallback(APIComponentProcessStateCallback callback,
                                   bool succeeded);

  // Sends a Mojo IPC message pipe invitation to the media analytics process.
  void SendMojoInvitation(APIComponentProcessStateCallback callback);

  // Callback for BootstrapMojoConnection D-Bus method calls to the media
  // analytics process.
  void OnBootstrapMojoConnection(APIComponentProcessStateCallback callback,
                                 bool succeeded);

  // This callback includes a mri::State so that the API manager can immediately
  // set the state of rtanalytics after start-up.
  void UpstartStartCallback(APIStateCallback callback,
                            const mri::State& state,
                            bool succeeded);

  // Callback for Upstart command to restart media analytics process.
  void UpstartRestartCallback(APIStateCallback callback, bool succeeded);

  // Callback for Upstart command to stop media analytics process.
  void UpstartStopProcessCallback(APIComponentProcessStateCallback callback,
                                  bool succeeded);

  void UpstartStopCallback(APIStateCallback callback, bool succeeded);

  // Callback with the mount point for a loaded component.
  void LoadComponentCallback(APISetAnalyticsComponentCallback callback,
                             const extensions::api::media_perception_private::
                                 ComponentInstallationError installation_error,
                             const base::FilePath& mount_point);

  bool ComponentIsLoaded();

  content::BrowserContext* const browser_context_;

  // Keeps track of whether the analytics process is running so that it can be
  // started with an Upstart D-Bus method call if necessary.
  AnalyticsProcessState analytics_process_state_;

  // Keeps track of the mount point for the current media analytics process
  // component from component updater. If this string is not set, no component
  // is set.
  std::string mount_point_;

  // Pointer to the MediaPerceptionService interface for communicating with the
  // service over Mojo.
  mojo::Remote<chromeos::media_perception::mojom::MediaPerceptionService>
      media_perception_service_;

  mojo::Remote<chromeos::media_perception::mojom::MediaPerceptionController>
      media_perception_controller_;

  std::unique_ptr<MediaPerceptionControllerClient>
      media_perception_controller_client_;

  ScopedObserver<chromeos::MediaAnalyticsClient,
                 chromeos::MediaAnalyticsClient::Observer>
      scoped_observer_{this};
  base::WeakPtrFactory<MediaPerceptionAPIManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionAPIManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_
