// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_browser_agent_base.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#pragma mark - OverlayBrowserAgentBase

OverlayBrowserAgentBase::OverlayBrowserAgentBase(Browser* browser)
    : installation_driver_(browser, this) {}

OverlayBrowserAgentBase::~OverlayBrowserAgentBase() = default;

#pragma mark - Protected

void OverlayBrowserAgentBase::AddInstaller(
    std::unique_ptr<OverlayRequestCallbackInstaller> installer,
    OverlayModality modality) {
  DCHECK(installer);
  CallbackInstallerStorage& storage = installer_storages_[modality];
  storage.installers.push_back(std::move(installer));
  // Reset the storage's request support to nullptr.  This will cause the
  // aggregate support for all callback installers added for `modality` to be
  // regenerated the next time GetRequestSupport() is called.
  storage.request_support = nullptr;
  // Notify the installation driver if this is the first installer added for
  // `modality`.
  if (storage.installers.size() == 1U)
    installation_driver_.StartInstallingCallbacks(modality);
}

#pragma mark Private

const OverlayRequestSupport* OverlayBrowserAgentBase::GetRequestSupport(
    OverlayModality modality) {
  CallbackInstallerStorage& storage = installer_storages_[modality];
  if (!storage.request_support) {
    const std::vector<std::unique_ptr<OverlayRequestCallbackInstaller>>&
        installers = storage.installers;
    std::vector<const OverlayRequestSupport*> supports(installers.size());
    for (size_t index = 0; index < installers.size(); ++index) {
      DCHECK(installers[index]->GetRequestSupport());
      supports[index] = installers[index]->GetRequestSupport();
    }
    storage.request_support = std::make_unique<OverlayRequestSupport>(supports);
  }
  return storage.request_support.get();
}

void OverlayBrowserAgentBase::InstallOverlayRequestCallbacks(
    OverlayRequest* request,
    OverlayModality modality) {
  for (auto& installer : installer_storages_[modality].installers) {
    installer->InstallCallbacks(request);
  }
}

#pragma mark - OverlayBrowserAgentBase::InstallationDriver

OverlayBrowserAgentBase::CallbackInstallationDriver::CallbackInstallationDriver(
    Browser* browser,
    OverlayBrowserAgentBase* browser_agent)
    : browser_(browser), browser_agent_(browser_agent) {
  DCHECK(browser_agent_);
  DCHECK(browser_);
}

OverlayBrowserAgentBase::CallbackInstallationDriver::
    ~CallbackInstallationDriver() = default;

void OverlayBrowserAgentBase::CallbackInstallationDriver::
    StartInstallingCallbacks(OverlayModality modality) {
  OverlayPresenter* presenter =
      OverlayPresenter::FromBrowser(browser_, modality);
  if (!scoped_observations_.IsObservingSource(presenter))
    scoped_observations_.AddObservation(presenter);
}

const OverlayRequestSupport*
OverlayBrowserAgentBase::CallbackInstallationDriver::GetRequestSupport(
    OverlayPresenter* presenter) const {
  return browser_agent_->GetRequestSupport(presenter->GetModality());
}

void OverlayBrowserAgentBase::CallbackInstallationDriver::WillShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    bool initial_presentation) {
  if (!initial_presentation)
    return;
  browser_agent_->InstallOverlayRequestCallbacks(request,
                                                 presenter->GetModality());
}

void OverlayBrowserAgentBase::CallbackInstallationDriver::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  scoped_observations_.RemoveObservation(presenter);
}

#pragma mark - OverlayBrowserAgentBase::CallbackInstallerStorage

OverlayBrowserAgentBase::CallbackInstallerStorage::CallbackInstallerStorage() =
    default;

OverlayBrowserAgentBase::CallbackInstallerStorage::~CallbackInstallerStorage() =
    default;
