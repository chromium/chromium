// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

// Implementation of Mojo PresentationService.
// It handles Presentation API requests coming from Blink / renderer process
// and delegates the requests to the embedder's media router via
// PresentationServiceDelegate.
// An instance of this class tied to a RenderFrameHost and listens to events
// related to the RFH via implementing WebContentsObserver.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first presentation API request is handled.
// This class currently handles requests from both controller and receiver
// frames. The sequence of calls from a controller looks like the following:
//   Create()
//   SetClient()
//   StartPresentation()
//   ...
// TODO(crbug.com/41336031): Split the controller and receiver logic into
// separate classes so that each is easier to reason about.
class CONTENT_EXPORT PresentationServiceImpl
    : public blink::mojom::PresentationService,
      public WebContentsObserver,
      public PresentationServiceDelegate::Observer {
 public:
  using NewPresentationCallback =
      base::OnceCallback<void(blink::mojom::PresentationConnectionResultPtr,
                              blink::mojom::PresentationErrorPtr)>;

  // Creates a PresentationServiceImpl using the given RenderFrameHost.
  static std::unique_ptr<PresentationServiceImpl> Create(
      RenderFrameHost* render_frame_host);

  PresentationServiceImpl(const PresentationServiceImpl&) = delete;
  PresentationServiceImpl& operator=(const PresentationServiceImpl&) = delete;

  ~PresentationServiceImpl() override;

  // Creates a binding between this object and |receiver|. Note that a
  // PresentationServiceImpl instance can be bound to multiple receivers.
  void Bind(mojo::PendingReceiver<blink::mojom::PresentationService> receiver);

  // PresentationService implementation.
  void SetDefaultPresentationUrls(
      const std::vector<GURL>& presentation_urls) override;
  void SetController(mojo::PendingRemote<blink::mojom::PresentationController>
                         presentation_controller_remote) override;
  void SetReceiver(mojo::PendingRemote<blink::mojom::PresentationReceiver>
                       presentation_receiver_remote) override;
  void ListenForScreenAvailability(const GURL& url) override;
  void StopListeningForScreenAvailability(const GURL& url) override;
  void StartPresentation(const std::vector<GURL>& presentation_urls,
                         NewPresentationCallback callback) override;
  void ReconnectPresentation(const std::vector<GURL>& presentation_urls,
                             const std::string& presentation_id,
                             NewPresentationCallback callback) override;
  void CloseConnection(const GURL& presentation_url,
                       const std::string& presentation_id) override;
  void Terminate(const GURL& presentation_url,
                 const std::string& presentation_id) override;

  void SetControllerDelegateForTesting(
      ControllerPresentationServiceDelegate* controller_delegate);

 private:
  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, OnDelegateDestroyed);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DelegateFails);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           SetDefaultPresentationUrlsNoopsOnNonMainFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionClose);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingStartPresentationRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingReconnectPresentationRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiverPresentationServiceDelegate);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiverDelegateOnSubFrame);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheBrowserTest,
                           PresentationConnectionClosed);

  // Maximum number of pending ReconnectPresentation requests at any given time.
  static const int kMaxQueuedRequests = 10;

  // Listener implementation owned by PresentationServiceImpl. An instance of
  // this is created when PresentationRequest.getAvailability() is resolved.
  // The instance receives screen availability results from the embedder and
  // propagates results back to PresentationServiceImpl.
  class CONTENT_EXPORT ScreenAvailabilityListenerImpl
      : public PresentationScreenAvailabilityListener {
   public:
    ScreenAvailabilityListenerImpl(const GURL& availability_url,
                                   PresentationServiceImpl* service);
    ~ScreenAvailabilityListenerImpl() override;

    // PresentationScreenAvailabilityListener implementation.
    GURL GetAvailabilityUrl() override;
    void OnScreenAvailabilityChanged(
        blink::mojom::ScreenAvailability availability) override;

   private:
    const GURL availability_url_;
    const raw_ptr<PresentationServiceImpl> service_;
  };

  // Ensures the provided NewPresentationCallback is invoked exactly once
  // before it goes out of scope.
  class NewPresentationCallbackWrapper {
   public:
    explicit NewPresentationCallbackWrapper(NewPresentationCallback callback);

    NewPresentationCallbackWrapper(const NewPresentationCallbackWrapper&) =
        delete;
    NewPresentationCallbackWrapper& operator=(
        const NewPresentationCallbackWrapper&) = delete;

    ~NewPresentationCallbackWrapper();

    void Run(blink::mojom::PresentationConnectionResultPtr result,
             blink::mojom::PresentationErrorPtr error);

   private:
    NewPresentationCallback callback_;
  };

  // Note: Use |PresentationServiceImpl::Create| instead. This constructor
  // should only be directly invoked in tests.
  // |render_frame_host|: The RFH this instance is associated with.
  // |web_contents|: The WebContents to observe.
  // |controller_delegate|: Where Presentation API requests are delegated to in
  // controller frame. Set to nullptr if current frame is receiver frame. Not
  // owned by this class.
  // |receiver_delegate|: Where Presentation API requests are delegated to in
  // receiver frame. Set to nullptr if current frame is controller frame. Not
  // owned by this class.
  PresentationServiceImpl(
      RenderFrameHost* render_frame_host,
      WebContents* web_contents,
      ControllerPresentationServiceDelegate* controller_delegate,
      ReceiverPresentationServiceDelegate* receiver_delegate);

  // WebContentsObserver override.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // PresentationServiceDelegate::Observer
  void OnDelegateDestroyed() override;

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when default presentation has started.
  void OnDefaultPresentationStarted(
      blink::mojom::PresentationConnectionResultPtr result);

  // Finds the callback from |pending_reconnect_presentation_cbs_| using
  // |request_id|.
  // If it exists, invoke it with |result| and |error|, then erase it
  // from |pending_reconnect_presentation_cbs_|. Returns true if the callback
  // was found.
  bool RunAndEraseReconnectPresentationMojoCallback(
      int request_id,
      blink::mojom::PresentationConnectionResultPtr result,
      blink::mojom::PresentationErrorPtr error);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartPresentationSucceeded(
      int request_id,
      blink::mojom::PresentationConnectionResultPtr result);
  void OnStartPresentationError(int request_id,
                                const blink::mojom::PresentationError& error);
  void OnReconnectPresentationSucceeded(
      int request_id,
      blink::mojom::PresentationConnectionResultPtr result);
  void OnReconnectPresentationError(
      int request_id,
      const blink::mojom::PresentationError& error);

  // Calls to |delegate_| to start listening for state changes for |connection|.
  // State changes will be returned via |OnConnectionStateChanged|.
  void ListenForConnectionStateChange(
      const blink::mojom::PresentationInfo& connection);

  // A callback registered to LocalPresentationManager when
  // the PresentationServiceImpl for the presentation receiver is initialized.
  // Calls |receiver_| to create a new PresentationConnection on receiver page.
  void OnReceiverConnectionAvailable(
      blink::mojom::PresentationConnectionResultPtr result);

  // Associates a ReconnectPresentation |callback| with a unique request ID and
  // stores it in a map. Moves out |callback| object if |callback| is registered
  // successfully. If the queue is full, returns a negative value and leaves
  // |callback| as is.
  int RegisterReconnectPresentationCallback(NewPresentationCallback* callback);

  // Invoked by the embedder's PresentationServiceDelegate when a
  // PresentationConnection's state has changed.
  void OnConnectionStateChanged(
      const blink::mojom::PresentationInfo& connection,
      const PresentationConnectionStateChangeInfo& info);

  // Returns true if this object is associated with |render_frame_host|.
  bool FrameMatches(content::RenderFrameHost* render_frame_host) const;

  // Invoked on Mojo connection error. Closes all Mojo message pipes held by
  // |this|.
  void OnConnectionError();

  // Returns |controller_delegate| if current frame is controller frame; Returns
  // |receiver_delegate| if current frame is receiver frame.
  PresentationServiceDelegate* GetPresentationServiceDelegate();

  // The RenderFrameHost associated with this object.
  const raw_ptr<RenderFrameHost> render_frame_host_;

  // Embedder-specific delegate for controller to forward Presentation requests
  // to. Must be nullptr if current page is receiver page or
  // embedder does not support Presentation API .
  raw_ptr<ControllerPresentationServiceDelegate> controller_delegate_;

  // Embedder-specific delegate for receiver to forward Presentation requests
  // to. Must be nullptr if current page is receiver page or
  // embedder does not support Presentation API.
  raw_ptr<ReceiverPresentationServiceDelegate> receiver_delegate_;

  // Pointer to the PresentationController implementation in the renderer.
  mojo::Remote<blink::mojom::PresentationController>
      presentation_controller_remote_;

  // Pointer to the PresentationReceiver implementation in the renderer.
  mojo::Remote<blink::mojom::PresentationReceiver>
      presentation_receiver_remote_;

  std::vector<GURL> default_presentation_urls_;

  using ScreenAvailabilityListenerMap =
      std::map<GURL, std::unique_ptr<ScreenAvailabilityListenerImpl>>;
  ScreenAvailabilityListenerMap screen_availability_listeners_;

  // For StartPresentation requests.
  // Set to a positive value when a StartPresentation request is being
  // processed.
  int start_presentation_request_id_;
  std::unique_ptr<NewPresentationCallbackWrapper>
      pending_start_presentation_cb_;

  // For ReconnectPresentation requests.
  std::unordered_map<int, std::unique_ptr<NewPresentationCallbackWrapper>>
      pending_reconnect_presentation_cbs_;

  mojo::ReceiverSet<blink::mojom::PresentationService>
      presentation_service_receivers_;

  // ID of the RenderFrameHost this object is associated with.
  int render_process_id_;
  int render_frame_id_;

  // If current frame is the outermost frame (not an iframe nor a fenced frame).
  bool is_outermost_document_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PresentationServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
