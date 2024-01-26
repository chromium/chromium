// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_

#import <map>
#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/overlay_browser_agent_base.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class InfobarInteractionHandler;

// Browser agent class that handles the model-layer updates for infobars.
class InfobarOverlayBrowserAgent
    : public OverlayBrowserAgentBase,
      public BrowserUserData<InfobarOverlayBrowserAgent> {
 public:
  ~InfobarOverlayBrowserAgent() override;

  // Adds an InfobarInteractionHandler to make model-layer updates for
  // interactions with infobars.  An OverlayCallbackInstaller will be created
  // from each added handler for each InfobarOverlayType.  `interaction_handler`
  // must not be null.  Only one interaction handler for a given InfobarType
  // can be added.
  void AddInfobarInteractionHandler(
      std::unique_ptr<InfobarInteractionHandler> interaction_handler);

  // Adds an InfobarInteractionHandler that corresponds to the given
  // `infobar_type`to make model-layer updates for interactions with infobars.
  // This method calls `AddInfobarInteractionHandler:` to create the
  // InfobarInteractionHandler.
  void AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType infobar_type);

 private:
  friend class BrowserUserData<InfobarOverlayBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Constructor used by CreateForBrowser().
  explicit InfobarOverlayBrowserAgent(Browser* browser);

  // Returns the interaction handler for the InfobarType of the infobar used to
  // configure `request`, or nullptr if `request` is not supported.
  InfobarInteractionHandler* GetInteractionHandler(OverlayRequest* request);

  // Helper object that notifies interaction handler of changes in infobar UI
  // visibility.
  class OverlayVisibilityObserver : public OverlayPresenterObserver {
   public:
    OverlayVisibilityObserver(Browser* browser,
                              InfobarOverlayBrowserAgent* browser_agent);
    ~OverlayVisibilityObserver() override;

   private:
    // Notifies the BrowserAgent's interaction handler that the visibility of
    // `request`'s UI has changed.
    void OverlayVisibilityChanged(OverlayRequest* request, bool visible);

    // OverlayPresenterObserver:
    const OverlayRequestSupport* GetRequestSupport(
        OverlayPresenter* presenter) const override;
    void DidShowOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void DidHideOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

    raw_ptr<InfobarOverlayBrowserAgent> browser_agent_ = nullptr;
    base::ScopedMultiSourceObservation<OverlayPresenter,
                                       OverlayPresenterObserver>
        scoped_observations_{this};
  };

  // The interaction handlers for each InfobarType.
  std::map<InfobarType, std::unique_ptr<InfobarInteractionHandler>>
      interaction_handlers_;
  // The observer for infobar banner presentations and dismissals.
  OverlayVisibilityObserver overlay_visibility_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_H_
