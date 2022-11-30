// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FUZZER_CONTROLLER_PRESENTATION_SERVICE_DELEGATE_FOR_FUZZING_H_
#define CONTENT_TEST_FUZZER_CONTROLLER_PRESENTATION_SERVICE_DELEGATE_FOR_FUZZING_H_

#include <map>
#include <memory>
#include <string>

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/test/fuzzer/controller_presentation_service_delegate_for_fuzzing.pb.h"
#include "media/base/flinging_controller.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

// Fake for the `ControllerPresentationServiceDelegate` interface
//
// This fake is limited to do callback, observer and listener behaviours,
// according to a set of actions specified by a protobuf.
// So it is not suitable for tests. It is designed for a fuzzer.
//
// Usage: call the component as expected as the delegate interface.
// To run the callables provided, use `NextAction`,
// provide a protobuf of the `Action` message - there lists the possible calls.
// This will instruct the fake to run the callbacks accordingly.
//
// The callables are run on the UI thread, as the delegate interface is expected
// to be called on the UI thread.
// In the case of MojoLPM fuzzers, `NextAction` should be called on the fuzzer
// thread.
class ControllerPresentationServiceDelegateForFuzzing
    : public content::ControllerPresentationServiceDelegate {
 public:
  ControllerPresentationServiceDelegateForFuzzing();
  ~ControllerPresentationServiceDelegateForFuzzing() override;

  // Used to run one callable.
  // These are set by using this as implementation of the delegate interface.
  // The callable is specified by the `action`, as defined in assocciated proto.
  //
  // All calls must be run on the same sequence
  // (for MojoLPM fuzzers, it should be the fuzzer thread)
  void NextAction(
      const content::fuzzing::
          controller_presentation_service_delegate_for_fuzzing::proto::Action&
              action);

  // ControllerPresentationServiceDelegate implementation
  void AddObserver(int render_process_id,
                   int render_frame_id,
                   Observer* observer) override;

  void RemoveObserver(int render_process_id, int render_frame_id) override;

  void Reset(int render_process_id, int render_frame_id) override;

  bool AddScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      content::PresentationScreenAvailabilityListener* listener) override;

  void RemoveScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      content::PresentationScreenAvailabilityListener* listener) override;

  void SetDefaultPresentationUrls(
      const content::PresentationRequest& request,
      content::DefaultPresentationConnectionCallback callback) override;

  void StartPresentation(
      const content::PresentationRequest& request,
      content::PresentationConnectionCallback success_cb,
      content::PresentationConnectionErrorCallback error_cb) override;

  void ReconnectPresentation(
      const content::PresentationRequest& request,
      const std::string& presentation_id,
      content::PresentationConnectionCallback success_cb,
      content::PresentationConnectionErrorCallback error_cb) override;

  void CloseConnection(int render_process_id,
                       int render_frame_id,
                       const std::string& presentation_id) override;

  void Terminate(int render_process_id,
                 int render_frame_id,
                 const std::string& presentation_id) override;

  std::unique_ptr<media::FlingingController> GetFlingingController(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_id) override;

  void ListenForConnectionStateChange(
      int render_process_id,
      int render_frame_id,
      const blink::mojom::PresentationInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb) override;

 private:
  using Action = content::fuzzing::
      controller_presentation_service_delegate_for_fuzzing::proto::Action;

  // Use to invoke the callbacks & registered callables
  // For each, we have two member functions, where the first calls the second.
  // The first is prefixed by `Handle`, which takes the raw protobuf datatype,
  // and is run on the same thread as `NextAction` (for MojoLPM: fuzzer thread).
  // The second is prefixed by `Call` which invokes the call with the converted
  // datatype, and is run on the UI thread.
  void HandleListenersGetAvailabilityUrl(
      const mojolpm::url::mojom::Url& proto_url);
  void CallListenersGetAvailabilityUrl(GURL url);

  void HandleListenersOnScreenAvailabilityChanged(
      const mojolpm::url::mojom::Url& proto_url,
      const mojolpm::blink::mojom::ScreenAvailability&
          proto_screen_availability);
  void CallListenersOnScreenAvailabilityChanged(
      GURL url,
      blink::mojom::ScreenAvailability screen_availability);

  void HandleSetDefaultPresentationUrls(
      const mojolpm::blink::mojom::PresentationConnectionResult& proto_result);
  void CallSetDefaultPresentationUrls(
      blink::mojom::PresentationConnectionResultPtr result_ptr);

  void HandleStartPresentationSuccess(
      const mojolpm::blink::mojom::PresentationConnectionResult& proto_result);
  void CallStartPresentationSuccess(
      blink::mojom::PresentationConnectionResultPtr result_ptr);

  void HandleStartPresentationError(
      const mojolpm::blink::mojom::PresentationError& proto_error);
  void CallStartPresentationError(blink::mojom::PresentationErrorPtr error_ptr);

  void HandleReconnectPresentationSuccess(
      const mojolpm::blink::mojom::PresentationConnectionResult& proto_result);
  void CallReconnectPresentationSuccess(
      blink::mojom::PresentationConnectionResultPtr result_ptr);

  void HandleReconnectPresentationError(
      const mojolpm::blink::mojom::PresentationError& proto_error);
  void CallReconnectPresentationError(
      blink::mojom::PresentationErrorPtr error_ptr);

  void HandleListenForConnectionStateChangeStateChanged(
      const mojolpm::blink::mojom::PresentationConnectionState&
          proto_connection_state);
  void CallListenForConnectionStateChangeStateChanged(
      blink::mojom::PresentationConnectionState connection_state);

  base::WeakPtr<ControllerPresentationServiceDelegateForFuzzing> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // We store all callbacks as single callables; not queued nor out of order,
  // as this mimics the real delegate as closely as possible
  std::map<content::GlobalRenderFrameHostId,
           content::PresentationServiceDelegate::Observer*>
      observers_;

  std::map<GURL, content::PresentationScreenAvailabilityListener*> listeners_;

  content::DefaultPresentationConnectionCallback
      set_default_presentation_urls_callback_;

  content::PresentationConnectionCallback start_presentation_success_cb_;

  content::PresentationConnectionErrorCallback start_presentation_error_cb_;

  content::PresentationConnectionCallback reconnect_presentation_success_cb_;

  content::PresentationConnectionErrorCallback reconnect_presentation_error_cb_;

  content::PresentationConnectionStateChangedCallback
      listen_for_connection_state_change_state_changed_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  // `PostTask`ing the calls onto the UI thread is not guaranteed to outlive
  // `this`. So a weak pointer is necessary for those calls.
  base::WeakPtrFactory<ControllerPresentationServiceDelegateForFuzzing>
      weak_factory_{this};
};

#endif  // CONTENT_TEST_FUZZER_CONTROLLER_PRESENTATION_SERVICE_DELEGATE_FOR_FUZZING_H_
