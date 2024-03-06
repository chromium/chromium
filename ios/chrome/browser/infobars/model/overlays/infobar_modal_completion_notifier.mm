// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_completion_notifier.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_callback_installer.h"

#pragma mark - InfobarModalCompletionNotifier

InfobarModalCompletionNotifier::InfobarModalCompletionNotifier(
    web::WebState* web_state)
    : callback_installer_(OverlayRequestQueueCallbackInstaller::Create(
          web_state,
          OverlayModality::kInfobarModal)) {
  callback_installer_->AddRequestCallbackInstaller(
      std::make_unique<ModalCompletionInstaller>(this));
}

InfobarModalCompletionNotifier::~InfobarModalCompletionNotifier() {
  for (auto& observer : observers_) {
    observer.InfobarModalCompletionNotifierDestroyed(this);
  }
}

void InfobarModalCompletionNotifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InfobarModalCompletionNotifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InfobarModalCompletionNotifier::ModalCompletionInstalled(
    InfoBarIOS* infobar) {
  active_modal_request_counts_[infobar]++;
}

void InfobarModalCompletionNotifier::ModalRequestCompleted(
    InfoBarIOS* infobar) {
  if (!--active_modal_request_counts_[infobar]) {
    for (auto& observer : observers_) {
      observer.InfobarModalsCompleted(this, infobar);
    }
    active_modal_request_counts_.erase(infobar);
  }
}

#pragma mark - InfobarModalCompletionNotifier::ModalCompletionInstaller

InfobarModalCompletionNotifier::ModalCompletionInstaller::
    ModalCompletionInstaller(InfobarModalCompletionNotifier* notifier)
    : notifier_(notifier), weak_factory_(this) {
  DCHECK(notifier_);
}

InfobarModalCompletionNotifier::ModalCompletionInstaller::
    ~ModalCompletionInstaller() = default;

void InfobarModalCompletionNotifier::ModalCompletionInstaller::ModalCompleted(
    InfoBarIOS* infobar,
    OverlayResponse* response) {
  notifier_->ModalRequestCompleted(infobar);
}

const OverlayRequestSupport*
InfobarModalCompletionNotifier::ModalCompletionInstaller::GetRequestSupport()
    const {
  return InfobarOverlayRequestConfig::RequestSupport();
}

void InfobarModalCompletionNotifier::ModalCompletionInstaller::
    InstallCallbacksInternal(OverlayRequest* request) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;

  request->GetCallbackManager()->AddCompletionCallback(base::BindOnce(
      &InfobarModalCompletionNotifier::ModalCompletionInstaller::ModalCompleted,
      weak_factory_.GetWeakPtr(), base::UnsafeDanglingUntriaged(infobar)));

  notifier_->ModalCompletionInstalled(infobar);
}
