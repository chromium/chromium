// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_IMPL_H_

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Mutable implementation of OverlayRequestQueue.
class OverlayRequestQueueImpl : public OverlayRequestQueue {
 public:
  ~OverlayRequestQueueImpl() override;

  // Container that stores the queues for each modality.  Usage example:
  //
  // OverlayRequestQueueImpl::Container::FromWebState(web_state)->
  //     QueueForModality(OverlayModality::kWebContentArea);
  class Container : public web::WebStateUserData<Container> {
   public:
    ~Container() override;
    // Returns the request queue for `modality`.
    OverlayRequestQueueImpl* QueueForModality(OverlayModality modality);

   private:
    friend class web::WebStateUserData<Container>;
    WEB_STATE_USER_DATA_KEY_DECL();
    Container(web::WebState* web_state);

    raw_ptr<web::WebState> web_state_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayRequestQueueImpl>> queues_;
  };

  // Delegate class for the queue.
  class Delegate {
   public:
    // Called when `request` is removed from `queue`.  `cancelled` is true if
    // the request is removed by its cancel handler or by a call to
    // CancelAllRequests().
    virtual void OverlayRequestRemoved(OverlayRequestQueueImpl* queue,
                                       std::unique_ptr<OverlayRequest> request,
                                       bool cancelled) = 0;
    // Called when the queue is about to replace the existing delegate.
    virtual void OverlayRequestQueueWillReplaceDelegate(
        OverlayRequestQueueImpl* queue) = 0;
  };

  // Observer class for the queue.
  class Observer : public base::CheckedObserver {
   public:
    // Called after `request` has been added to `queue`.
    virtual void RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                                     OverlayRequest* request,
                                     size_t index) {}

    // Called when `queue` is about to be destroyed.
    virtual void OverlayRequestQueueDestroyed(OverlayRequestQueueImpl* queue) {}
  };

  // Returns the request queue implementation for `web_state` at `modality`.
  static OverlayRequestQueueImpl* FromWebState(web::WebState* web_state,
                                               OverlayModality modality);

  // Create the request queue implementation for `web_state`.
  static void CreateForWebState(web::WebState* web_state);

  // Sets the delegate.
  void SetDelegate(Delegate* delegate);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a weak pointer to the queue.
  base::WeakPtr<OverlayRequestQueueImpl> GetWeakPtr();

  // Removes the front request from the queue, transferring ownership of the
  // request to queue's delegate.  Must be called on a non-empty queue.
  void PopFrontRequest();

  // OverlayRequestQueue:
  size_t size() const override;
  OverlayRequest* front_request() const override;
  OverlayRequest* GetRequest(size_t index) const override;
  void AddRequest(std::unique_ptr<OverlayRequest> request,
                  std::unique_ptr<OverlayRequestCancelHandler> cancel_handler =
                      nullptr) override;
  void InsertRequest(size_t index,
                     std::unique_ptr<OverlayRequest> request,
                     std::unique_ptr<OverlayRequestCancelHandler>
                         cancel_handler = nullptr) override;
  void CancelAllRequests() override;
  void CancelRequest(OverlayRequest* request) override;

 private:
  // Helper object that stores OverlayRequests along with their cancellation
  // handlers.
  struct OverlayRequestStorage {
    OverlayRequestStorage(
        std::unique_ptr<OverlayRequest> request,
        std::unique_ptr<OverlayRequestCancelHandler> cancel_handler);
    OverlayRequestStorage(OverlayRequestStorage&& storage);
    ~OverlayRequestStorage();

    std::unique_ptr<OverlayRequest> request;
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler;
  };

  // Private constructor called by container.
  explicit OverlayRequestQueueImpl(web::WebState* web_state);

  // Removes the request at `index`, passing ownership of the removed request to
  // the delegate.  `cancelled` is true if the request is removed by its cancel
  // handler or by a call to CancelAllRequests().
  void RemoveRequest(size_t index, bool cancelled);

  raw_ptr<web::WebState> web_state_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;
  base::ObserverList<Observer, /* check_empty= */ true> observers_;
  // The queue used to hold the received requests.  Stored as a circular dequeue
  // to allow performant pop events from the front of the queue.
  base::circular_deque<OverlayRequestStorage> request_storages_;
  base::WeakPtrFactory<OverlayRequestQueueImpl> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_IMPL_H_
