// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/infobar_overlay_browser_agent.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/default/default_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"

#pragma mark - InfobarOverlayBrowserAgent

BROWSER_USER_DATA_KEY_IMPL(InfobarOverlayBrowserAgent)

InfobarOverlayBrowserAgent::InfobarOverlayBrowserAgent(Browser* browser)
    : OverlayBrowserAgentBase(browser),
      overlay_visibility_observer_(browser, this) {}

InfobarOverlayBrowserAgent::~InfobarOverlayBrowserAgent() = default;

#pragma mark Public

void InfobarOverlayBrowserAgent::AddInfobarInteractionHandler(
    std::unique_ptr<InfobarInteractionHandler> interaction_handler) {
  // Only one installer should be set for a single request type.
  InfobarType type = interaction_handler->infobar_type();
  DCHECK(!interaction_handlers_[type]);
  // Add the banner installer.  Every InfobarType supports banners, so the added
  // installer is gauranteed to be non-null.
  AddInstaller(interaction_handler->CreateBannerCallbackInstaller(),
               OverlayModality::kInfobarBanner);
  // Add the modal installers.  Not all InfobarTypes support modal UI, so the
  // installer must be checked before being added.
  std::unique_ptr<OverlayRequestCallbackInstaller> modal_installer =
      interaction_handler->CreateModalCallbackInstaller();
  if (modal_installer) {
    AddInstaller(std::move(modal_installer), OverlayModality::kInfobarModal);
  }
  // Add the interaction handler to the list.
  interaction_handlers_[type] = std::move(interaction_handler);
}

void InfobarOverlayBrowserAgent::
    AddDefaultInfobarInteractionHandlerForInfobarType(
        InfobarType infobar_type) {
  AddInfobarInteractionHandler(std::make_unique<InfobarInteractionHandler>(
      infobar_type,
      std::make_unique<DefaultInfobarBannerInteractionHandler>(infobar_type),
      /*modal_handler=*/nullptr));
}

#pragma mark Private

InfobarInteractionHandler* InfobarOverlayBrowserAgent::GetInteractionHandler(
    OverlayRequest* request) {
  if (!request)
    return nullptr;
  return interaction_handlers_[GetOverlayRequestInfobarType(request)].get();
}

#pragma mark - InfobarOverlayBrowserAgent::OverlayVisibilityObserver

InfobarOverlayBrowserAgent::OverlayVisibilityObserver::
    OverlayVisibilityObserver(Browser* browser,
                              InfobarOverlayBrowserAgent* browser_agent)
    : browser_agent_(browser_agent) {
  DCHECK(browser_agent_);
  scoped_observations_.AddObservation(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarBanner));
  scoped_observations_.AddObservation(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarModal));
}

InfobarOverlayBrowserAgent::OverlayVisibilityObserver::
    ~OverlayVisibilityObserver() = default;

void InfobarOverlayBrowserAgent::OverlayVisibilityObserver::
    OverlayVisibilityChanged(OverlayRequest* request, bool visible) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  browser_agent_->GetInteractionHandler(request)->InfobarVisibilityChanged(
      infobar, GetOverlayRequestInfobarOverlayType(request), visible);
}

const OverlayRequestSupport*
InfobarOverlayBrowserAgent::OverlayVisibilityObserver::GetRequestSupport(
    OverlayPresenter* presenter) const {
  return browser_agent_->GetRequestSupport(presenter->GetModality());
}

void InfobarOverlayBrowserAgent::OverlayVisibilityObserver::DidShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  OverlayVisibilityChanged(request, /*visible=*/true);
}

void InfobarOverlayBrowserAgent::OverlayVisibilityObserver::DidHideOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  OverlayVisibilityChanged(request, /*visible=*/false);
}

void InfobarOverlayBrowserAgent::OverlayVisibilityObserver::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  scoped_observations_.RemoveObservation(presenter);
}
