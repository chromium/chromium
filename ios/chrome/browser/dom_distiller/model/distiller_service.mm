// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/core/distiller.h"
#import "components/prefs/pref_service.h"

DistillerService::DistillerService(
    std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
    PrefService* pref_service)
    : distiller_factory_(std::move(distiller_factory)),
      distilled_page_prefs_(
          std::make_unique<dom_distiller::DistilledPagePrefs>(pref_service)) {}

DistillerService::~DistillerService() = default;

void DistillerService::DistillPage(
    const GURL& url,
    std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
    dom_distiller::Distiller::DistillationFinishedCallback finished_cb,
    const dom_distiller::Distiller::DistillationUpdateCallback& update_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (distiller_) {
    // There is already a distillation in progress, enqueue the request.
    pending_requests_.emplace_back(url, std::move(distiller_page),
                                   std::move(finished_cb), update_cb);
    return;
  }

  distiller_ = distiller_factory_->CreateDistiller();
  distiller_->DistillPage(
      url, std::move(distiller_page),
      std::move(finished_cb)
          .Then(base::BindOnce(&DistillerService::DistillationFinished,
                               weak_ptr_factory_.GetWeakPtr())),
      update_cb);
}

dom_distiller::DistilledPagePrefs* DistillerService::GetDistilledPagePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return distilled_page_prefs_.get();
}

void DistillerService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_requests_.clear();
  distiller_.reset();
}

void DistillerService::DistillationFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // dom_distiller::Distiller asserts that the object is not deleted as part
  // of calling the finished callback. Thus it is not possible to delete the
  // object here. Instead schedule a call to ProcessNextDistillation() where
  // the deletion will be performed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DistillerService::ProcessNextDistillation,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DistillerService::ProcessNextDistillation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  distiller_.reset();

  // The queue may be empty since this method is also taking care of destroying
  // the dom_distiller::Distiller, since it is not possible to do it in the
  // DistillationFinished() method (see the comment there). Graciously handle
  // an empty queue by doing nothing.
  if (!pending_requests_.empty()) {
    Request request = std::move(pending_requests_.front());
    pending_requests_.pop_front();

    DistillPage(request.url, std::move(request.distiller_page),
                std::move(request.finished_cb), std::move(request.update_cb));
  }
}

DistillerService::Request::Request(
    GURL url,
    std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
    dom_distiller::Distiller::DistillationFinishedCallback finished_cb,
    dom_distiller::Distiller::DistillationUpdateCallback update_cb)
    : url(std::move(url)),
      distiller_page(std::move(distiller_page)),
      finished_cb(std::move(finished_cb)),
      update_cb(std::move(update_cb)) {}

DistillerService::Request::Request(Request&&) = default;

DistillerService::Request& DistillerService::Request::operator=(Request&&) =
    default;

DistillerService::Request::~Request() = default;
