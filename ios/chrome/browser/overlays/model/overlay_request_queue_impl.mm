// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_request_queue_impl.h"

#import <utility>

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/overlays/model/default_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/overlay_request_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/web/public/navigation/navigation_context.h"

#pragma mark - Factory method

// static
OverlayRequestQueue* OverlayRequestQueue::FromWebState(
    web::WebState* web_state,
    OverlayModality modality) {
  return OverlayRequestQueueImpl::FromWebState(web_state, modality);
}

// static
void OverlayRequestQueue::CreateForWebState(web::WebState* web_state) {
  OverlayRequestQueueImpl::CreateForWebState(web_state);
}

#pragma mark - OverlayRequestQueueImpl::Container

WEB_STATE_USER_DATA_KEY_IMPL(OverlayRequestQueueImpl::Container)

OverlayRequestQueueImpl::Container::Container(web::WebState* web_state)
    : web_state_(web_state) {}
OverlayRequestQueueImpl::Container::~Container() = default;

OverlayRequestQueueImpl* OverlayRequestQueueImpl::Container::QueueForModality(
    OverlayModality modality) {
  auto& queue = queues_[modality];
  if (!queue)
    queue = base::WrapUnique(new OverlayRequestQueueImpl(web_state_));
  return queue.get();
}

#pragma mark - OverlayRequestQueueImpl

OverlayRequestQueueImpl* OverlayRequestQueueImpl::FromWebState(
    web::WebState* web_state,
    OverlayModality modality) {
  auto* container = OverlayRequestQueueImpl::Container::FromWebState(web_state);
  CHECK(container)
      << "OverlayRequestQueue::CreateForWebState(...) must be called before "
         "OverlayRequestQueue::FromWebState(...)";
  return container->QueueForModality(modality);
}

void OverlayRequestQueueImpl::CreateForWebState(web::WebState* web_state) {
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
}

OverlayRequestQueueImpl::OverlayRequestQueueImpl(web::WebState* web_state)
    : web_state_(web_state), weak_factory_(this) {}

OverlayRequestQueueImpl::~OverlayRequestQueueImpl() {
  for (auto& observer : observers_) {
    observer.OverlayRequestQueueDestroyed(this);
  }
  CancelAllRequests();
}

#pragma mark Public

void OverlayRequestQueueImpl::SetDelegate(Delegate* delegate) {
  if (delegate_ == delegate)
    return;
  if (delegate_)
    delegate_->OverlayRequestQueueWillReplaceDelegate(this);
  delegate_ = delegate;
}

void OverlayRequestQueueImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OverlayRequestQueueImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<OverlayRequestQueueImpl> OverlayRequestQueueImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OverlayRequestQueueImpl::PopFrontRequest() {
  RemoveRequest(/*index=*/0, /*cancelled=*/false);
}

#pragma mark OverlayRequestQueue

size_t OverlayRequestQueueImpl::size() const {
  return request_storages_.size();
}

OverlayRequest* OverlayRequestQueueImpl::front_request() const {
  return size() ? GetRequest(0) : nullptr;
}

OverlayRequest* OverlayRequestQueueImpl::GetRequest(size_t index) const {
  DCHECK_LT(index, size());
  return request_storages_[index].request.get();
}

void OverlayRequestQueueImpl::AddRequest(
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler) {
  InsertRequest(size(), std::move(request), std::move(cancel_handler));
}

void OverlayRequestQueueImpl::InsertRequest(
    size_t index,
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler) {
  DCHECK_LE(index, size());
  DCHECK(request.get());
  // Create the cancel handler if necessary.
  if (!cancel_handler) {
    cancel_handler = std::make_unique<DefaultOverlayRequestCancelHandler>(
        request.get(), this, web_state_);
  }
  static_cast<OverlayRequestImpl*>(request.get())
      ->set_queue_web_state(web_state_);
  request_storages_.emplace(request_storages_.begin() + index,
                            std::move(request), std::move(cancel_handler));
  for (auto& observer : observers_) {
    observer.RequestAddedToQueue(this, request_storages_[index].request.get(),
                                 index);
  }
}

void OverlayRequestQueueImpl::CancelAllRequests() {
  while (size()) {
    // Requests are cancelled in reverse order to prevent attempting to present
    // subsequent requests after the dismissal of the front request's UI.
    RemoveRequest(/*index=*/size() - 1, /*cancelled=*/true);
  }
}

void OverlayRequestQueueImpl::CancelRequest(OverlayRequest* request) {
  for (size_t index = 0; index < size(); ++index) {
    if (request_storages_[index].request.get() == request) {
      RemoveRequest(index, /*cancelled=*/true);
      return;
    }
  }
}

#pragma mark Private

void OverlayRequestQueueImpl::RemoveRequest(size_t index, bool cancelled) {
  DCHECK_LT(index, size());
  auto iter = request_storages_.begin() + index;
  std::unique_ptr<OverlayRequest> request = std::move((*iter).request);
  request_storages_.erase(iter);
  if (delegate_)
    delegate_->OverlayRequestRemoved(this, std::move(request), cancelled);
}

#pragma mark OverlayRequestStorage

OverlayRequestQueueImpl::OverlayRequestStorage::OverlayRequestStorage(
    std::unique_ptr<OverlayRequest> request,
    std::unique_ptr<OverlayRequestCancelHandler> cancel_handler)
    : request(std::move(request)), cancel_handler(std::move(cancel_handler)) {}

OverlayRequestQueueImpl::OverlayRequestStorage::OverlayRequestStorage(
    OverlayRequestQueueImpl::OverlayRequestStorage&& storage)
    : request(std::move(storage.request)),
      cancel_handler(std::move(storage.cancel_handler)) {}

OverlayRequestQueueImpl::OverlayRequestStorage::~OverlayRequestStorage() {}
