// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/base/flinging_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace content {

struct PresentationRequest;
class PresentationScreenAvailabilityListener;

using PresentationConnectionCallback =
    base::OnceCallback<void(blink::mojom::PresentationConnectionResultPtr)>;
using PresentationConnectionErrorCallback =
    base::OnceCallback<void(const blink::mojom::PresentationError&)>;
using DefaultPresentationConnectionCallback = base::RepeatingCallback<void(
    blink::mojom::PresentationConnectionResultPtr)>;

struct PresentationConnectionStateChangeInfo {
  explicit PresentationConnectionStateChangeInfo(
      blink::mojom::PresentationConnectionState state)
      : state(state),
        close_reason(
            blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR) {
  }
  ~PresentationConnectionStateChangeInfo() = default;

  blink::mojom::PresentationConnectionState state;

  // |close_reason| and |messsage| are only used for state change to CLOSED.
  blink::mojom::PresentationConnectionCloseReason close_reason;
  std::string message;
};

using PresentationConnectionStateChangedCallback =
    base::RepeatingCallback<void(const PresentationConnectionStateChangeInfo&)>;

using ReceiverConnectionAvailableCallback = base::RepeatingCallback<void(
    blink::mojom::PresentationConnectionResultPtr)>;

// Base class for ControllerPresentationServiceDelegate and
// ReceiverPresentationServiceDelegate.
class CONTENT_EXPORT PresentationServiceDelegate {
 public:
  // Observer interface to listen for changes to PresentationServiceDelegate.
  class CONTENT_EXPORT Observer {
   public:
    // Called when the PresentationServiceDelegate is being destroyed.
    virtual void OnDelegateDestroyed() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~PresentationServiceDelegate() {}

  // Registers an observer associated with frame with |render_process_id|
  // and |render_frame_id| with this class to listen for updates.
  // This class does not own the observer.
  // It is an error to add an observer if there is already an observer for that
  // frame.
  virtual void AddObserver(int render_process_id,
                           int render_frame_id,
                           Observer* observer) = 0;

  // Unregisters the observer associated with the frame with |render_process_id|
  // and |render_frame_id|.
  // The observer will no longer receive updates.
  virtual void RemoveObserver(int render_process_id, int render_frame_id) = 0;

  // Resets the presentation state for the frame given by |render_process_id|
  // and |render_frame_id|.
  // This unregisters all screen availability associated with the given frame,
  // and clears the default presentation URL for the frame.
  virtual void Reset(int render_process_id, int render_frame_id) = 0;
};

// An interface implemented by embedders to handle Presentation API calls
// forwarded from PresentationServiceImpl.
class CONTENT_EXPORT ControllerPresentationServiceDelegate
    : public PresentationServiceDelegate {
 public:
  using SendMessageCallback = base::OnceCallback<void(bool)>;

  // Registers |listener| to continuously listen for
  // availability updates for a presentation URL, originated from the frame
  // given by |render_process_id| and |render_frame_id|.
  // This class does not own |listener|.
  // Returns true on success.
  // This call will return false if a listener with the same presentation URL
  // from the same frame is already registered.
  virtual bool AddScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Unregisters |listener| originated from the frame given by
  // |render_process_id| and |render_frame_id| from this class. The listener
  // will no longer receive availability updates.
  virtual void RemoveScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Sets the default presentation URLs represented by |request|. When the
  // default presentation is started on this frame, |callback| will be invoked
  // with the corresponding blink::mojom::PresentationInfo object.
  // If |request.presentation_urls| is empty, the default presentation URLs will
  // be cleared and the previously registered callback (if any) will be removed.
  virtual void SetDefaultPresentationUrls(
      const content::PresentationRequest& request,
      DefaultPresentationConnectionCallback callback) = 0;

  // Starts a new presentation.
  // |request.presentation_urls| contains a list of possible URLs for the
  // presentation. Typically, the embedder will allow the user to select a
  // screen to show one of the URLs.
  // |request|: The request to start a presentation.
  // |success_cb|: Invoked with presentation info, if presentation started
  // successfully.
  // |error_cb|: Invoked with error reason, if presentation did not
  // start.
  virtual void StartPresentation(
      const content::PresentationRequest& request,
      PresentationConnectionCallback success_cb,
      PresentationConnectionErrorCallback error_cb) = 0;

  // Reconnects to an existing presentation. Unlike StartPresentation(), this
  // does not bring a screen list UI.
  // |request|: The request to reconnect to a presentation.
  // |presentation_id|: The ID of the presentation to reconnect.
  // |success_cb|: Invoked with presentation info, if presentation reconnected
  // successfully.
  // |error_cb|: Invoked with error reason, if reconnection failed.
  virtual void ReconnectPresentation(
      const content::PresentationRequest& request,
      const std::string& presentation_id,
      PresentationConnectionCallback success_cb,
      PresentationConnectionErrorCallback error_cb) = 0;

  // Closes an existing presentation connection.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_id|: The ID of the presentation to close.
  virtual void CloseConnection(int render_process_id,
                               int render_frame_id,
                               const std::string& presentation_id) = 0;

  // Terminates an existing presentation.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_id|: The ID of the presentation to terminate.
  virtual void Terminate(int render_process_id,
                         int render_frame_id,
                         const std::string& presentation_id) = 0;

  // Gets a FlingingController for a given presentation ID.
  // |render_process_id|, |render_frame_id|: ID of originating frame.
  // |presentation_id|: The ID of the presentation for which we want a
  // Controller.
  virtual std::unique_ptr<media::FlingingController> GetFlingingController(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_id) = 0;

  // Continuously listen for state changes for a PresentationConnection in a
  // frame.
  // |render_process_id|, |render_frame_id|: ID of frame.
  // |connection|: PresentationConnection to listen for state changes.
  // |state_changed_cb|: Invoked with the PresentationConnection and its new
  // state whenever there is a state change.
  virtual void ListenForConnectionStateChange(
      int render_process_id,
      int render_frame_id,
      const blink::mojom::PresentationInfo& connection,
      const PresentationConnectionStateChangedCallback& state_changed_cb) = 0;
};

// An interface implemented by embedders to handle
// PresentationService calls from a presentation receiver.
class CONTENT_EXPORT ReceiverPresentationServiceDelegate
    : public PresentationServiceDelegate {
 public:
  // Registers a callback from the embedder when an offscreeen presentation has
  // been successfully started.
  // |receiver_available_callback|: Invoked when successfully starting a
  // local presentation.
  virtual void RegisterReceiverConnectionAvailableCallback(
      const content::ReceiverConnectionAvailableCallback&
          receiver_available_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
