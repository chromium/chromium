// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_

#include <memory>

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"

class InfoBarIOS;

// Helper object, intended to be subclassed, that encapsulates the model-layer
// updates required for interaction with each type of UI used to display an
// infobar.  Subclasses should be created for each InfobarType to manage the
// user interaction for InfoBars of that type.
class InfobarInteractionHandler {
 public:
  // Helper object used by InfobarInteractionHandler to handle interaction for
  // a single InfobarOverlayType.
  class Handler {
   public:
    Handler() = default;
    virtual ~Handler() = default;

    // Creates a callback installer used to make model-layer updates for this
    // handler's InfobarOverlayType.
    virtual std::unique_ptr<OverlayRequestCallbackInstaller>
    CreateInstaller() = 0;
    // Notifies the handler that `infobar`'s UI with the handler's InfobarType
    virtual void InfobarVisibilityChanged(InfoBarIOS* infobar,
                                          bool visible) = 0;
  };

  // Constructor for an InfobarInteractionHandler that uses the provided
  // handlers for each InfobarOverlayType.  `banner_handler` must be non-null.
  // `modal_handler` may be null if its corresponding InfobarOverlayType is not
  // supported for `infobar_type`.
  InfobarInteractionHandler(InfobarType infobar_type,
                            std::unique_ptr<Handler> banner_handler,
                            std::unique_ptr<Handler> modal_handler);
  virtual ~InfobarInteractionHandler();

  // Returns the InfobarType whose interactions are handled by this instance.
  InfobarType infobar_type() const { return infobar_type_; }

  // Creates an OverlayRequestCallbackInstaller that handles model-layer updates
  // the the infobar's banner UI.  Guaranteed to be non-null.
  std::unique_ptr<OverlayRequestCallbackInstaller>
  CreateBannerCallbackInstaller();

  // Creates an OverlayRequestCallbackInstaller that handles model-layer updates
  // the the infobar's modal UI.  Returns null  if modals are not
  // supported for this InfobarType.
  std::unique_ptr<OverlayRequestCallbackInstaller>
  CreateModalCallbackInstaller();

  // Called to notify the interaction handler that `infobar`'s overlay UI with
  // `overlay_type`'s visibility has changed.
  void InfobarVisibilityChanged(InfoBarIOS* infobar,
                                InfobarOverlayType overlay_type,
                                bool visible);

 protected:
  // The type of infobar whose interactions are handled by this instance.
  InfobarType infobar_type_;
  // The handlers for each InfobarOverlayType.
  std::unique_ptr<Handler> banner_handler_;
  std::unique_ptr<Handler> modal_handler_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_INFOBAR_INTERACTION_HANDLER_H_
