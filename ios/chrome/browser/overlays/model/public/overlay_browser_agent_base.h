// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_BROWSER_AGENT_BASE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_BROWSER_AGENT_BASE_H_

#include <memory>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#include "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

// Browser agent class, intended to be subclassed, that installs callbacks for
// OverlayRequests in order to make model-layer updates for interaction with
// overlay UI.  Used to combine model-layer logic between OverlayRequests with
// shared functionality but different OverlayRequestConfigs (e.g. an action
// sheet and alert overlay that both update the same model object).  Subclasses
// should also specialize the BrowserUserData template so each subclass has its
// own unique key.
class OverlayBrowserAgentBase {
 public:
  virtual ~OverlayBrowserAgentBase();

 protected:
  // Constructor to be called by subclasses.
  explicit OverlayBrowserAgentBase(Browser* browser);

  // Called by subclasses in order to install callbacks using `installer` on
  // supported OverlayRequests at `modality`.  `installer` must be non-null.
  void AddInstaller(std::unique_ptr<OverlayRequestCallbackInstaller> installer,
                    OverlayModality modality);

  // Returns the aggregate request support for all callback installers added
  // for `modality`.
  const OverlayRequestSupport* GetRequestSupport(OverlayModality modality);

 private:
  // Installs the callbacks for `request` using the installers added via
  // AddInstaller() for `modality`.
  void InstallOverlayRequestCallbacks(OverlayRequest* request,
                                      OverlayModality modality);

  // Helper object that drives the installation of callbacks for each
  // OverlayRequest upon the first presentation of its overlay UI.
  class CallbackInstallationDriver : public OverlayPresenterObserver {
   public:
    // Constructor for an installation driver for OverlayRequests presented via
    // `browser`'s OverlayPresenters.
    CallbackInstallationDriver(Browser* browser,
                               OverlayBrowserAgentBase* browser_agent);
    ~CallbackInstallationDriver() override;

    // Starts driving the BrowserAgent's installation of callbacks for requests
    // whose UI is presented from `modality`'s presenter.
    void StartInstallingCallbacks(OverlayModality modality);

   private:
    // OverlayPresenterObserver:
    const OverlayRequestSupport* GetRequestSupport(
        OverlayPresenter* presenter) const override;
    void WillShowOverlay(OverlayPresenter* presenter,
                         OverlayRequest* request,
                         bool initial_presentation) override;
    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

    raw_ptr<Browser> browser_ = nullptr;
    raw_ptr<OverlayBrowserAgentBase> browser_agent_ = nullptr;
    base::ScopedMultiSourceObservation<OverlayPresenter,
                                       OverlayPresenterObserver>
        scoped_observations_{this};
  };

  // Storage struct used to store OverlayRequestCallbackInstallers and their
  // aggregated support for each OverlayModality.
  struct CallbackInstallerStorage {
    CallbackInstallerStorage();
    ~CallbackInstallerStorage();
    // The callback installers
    std::vector<std::unique_ptr<OverlayRequestCallbackInstaller>> installers;
    // The aggregate support for all added installers.
    std::unique_ptr<OverlayRequestSupport> request_support;
  };

  // Map containing the list of OverlayRequestCallbackInstallers for each
  // OverlayModality.
  std::map<OverlayModality, CallbackInstallerStorage> installer_storages_;
  // The callback installation driver.
  CallbackInstallationDriver installation_driver_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_BROWSER_AGENT_BASE_H_
