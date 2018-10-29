// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_scheduler.h"

#include "base/guid.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"

namespace content {

BackgroundFetchScheduler::Controller::Controller(
    BackgroundFetchScheduler* scheduler,
    const BackgroundFetchRegistrationId& registration_id,
    FinishedCallback finished_callback)
    : scheduler_(scheduler),
      registration_id_(registration_id),
      finished_callback_(std::move(finished_callback)) {
  DCHECK(scheduler_);
  DCHECK(finished_callback_);
}

BackgroundFetchScheduler::Controller::~Controller() = default;

void BackgroundFetchScheduler::Controller::Finish(
    blink::mojom::BackgroundFetchFailureReason reason_to_abort) {
  DCHECK(reason_to_abort != blink::mojom::BackgroundFetchFailureReason::NONE ||
         !HasMoreRequests());

  scheduler_->RemoveJobController(this);

  // Developer-initiated abortions will have already marked the registration for
  // deletion, so make sure that we don't execute the same code-path twice.
  if (reason_to_abort ==
      blink::mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER) {
    return;
  }

  // Race conditions make it possible for a controller to finish twice. This
  // should be removed when the scheduler starts owning the controllers.
  if (!finished_callback_)
    return;

  std::move(finished_callback_).Run(registration_id_, reason_to_abort);
}

BackgroundFetchScheduler::BackgroundFetchScheduler(
    RequestProvider* request_provider)
    : request_provider_(request_provider) {}

BackgroundFetchScheduler::~BackgroundFetchScheduler() = default;

void BackgroundFetchScheduler::AddJobController(Controller* controller) {
  DCHECK(controller);
  controller_queue_.push_back(controller);

  std::vector<scoped_refptr<content::BackgroundFetchRequestInfo>>
      outstanding_requests = controller->TakeOutstandingRequests();
  // There are active downloads from the previous session.
  if (!outstanding_requests.empty()) {
    // The current assumption is that there can be at most one active fetch
    // at any given time.
    DCHECK(!active_controller_);
    DCHECK_EQ(outstanding_requests.size(), 1u);

    active_controller_ = controller;
    for (auto& request_info : outstanding_requests) {
      active_controller_->StartRequest(
          std::move(request_info),
          base::BindOnce(&BackgroundFetchScheduler::MarkRequestAsComplete,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (!active_controller_)
    ScheduleDownload();
}

void BackgroundFetchScheduler::RemoveJobController(Controller* controller) {
  DCHECK(controller);

  base::EraseIf(controller_queue_, [controller](Controller* queued_controller) {
    return controller == queued_controller;
  });

  if (active_controller_ != controller)
    return;

  // TODO(peter): Move cancellation of requests to the scheduler.
  active_controller_ = nullptr;

  ScheduleDownload();
}

void BackgroundFetchScheduler::ScheduleDownload() {
  DCHECK(!active_controller_);
  if (controller_queue_.empty())
    return;

  active_controller_ = controller_queue_.front();
  controller_queue_.pop_front();

  request_provider_->PopNextRequest(
      active_controller_->registration_id(),
      base::BindOnce(&BackgroundFetchScheduler::DidPopNextRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchScheduler::DidPopNextRequest(
    blink::mojom::BackgroundFetchError error,
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  // It's possible for the |active_controller_| to have been aborted while the
  // next request was being retrieved. Bail out when that happens.
  if (!active_controller_) {
    ScheduleDownload();
    return;
  }

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    // This fetch is being abandoned, after which something will be scheduled.
    return;
  }

  DCHECK(request_info);

  // Otherwise start the |request_info| through the live Job Controller.
  active_controller_->StartRequest(
      std::move(request_info),
      base::BindOnce(&BackgroundFetchScheduler::MarkRequestAsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchScheduler::MarkRequestAsComplete(
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  // It's possible for the |active_controller_| to have been aborted while the
  // request was being started. Bail out in that case.
  if (!active_controller_)
    return;

  request_provider_->MarkRequestAsComplete(
      active_controller_->registration_id(), std::move(request_info),
      base::BindOnce(&BackgroundFetchScheduler::DidMarkRequestAsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchScheduler::DidMarkRequestAsComplete(
    blink::mojom::BackgroundFetchError error) {
  // It's possible for the |active_controller_| to have been aborted while the
  // request was being marked as completed. Bail out in that case.
  if (!active_controller_)
    return;

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    // This fetch is being abandoned, after which something will be scheduled.
    DCHECK_EQ(error, blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }

  // Continue with the |active_controller_| while there are files pending.
  if (active_controller_->HasMoreRequests()) {
    request_provider_->PopNextRequest(
        active_controller_->registration_id(),
        base::BindOnce(&BackgroundFetchScheduler::DidPopNextRequest,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  active_controller_->Finish(blink::mojom::BackgroundFetchFailureReason::NONE);
}

}  // namespace content
