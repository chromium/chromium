// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"

#import "components/dom_distiller/core/distiller.h"

DistillerService::DistillerService(
    std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory)
    : distiller_factory_(std::move(distiller_factory)) {}

DistillerService::~DistillerService() = default;

void DistillerService::DistillPage(
    const GURL& url,
    std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
    dom_distiller::Distiller::DistillationFinishedCallback finished_cb,
    const dom_distiller::Distiller::DistillationUpdateCallback& update_cb) {
  distiller_ = distiller_factory_->CreateDistiller();
  distiller_->DistillPage(url, std::move(distiller_page),
                          std::move(finished_cb), update_cb);
}

void DistillerService::Shutdown() {}
