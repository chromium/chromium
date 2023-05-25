// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

// Default helper object that updates the model layer for interaction events
// with the infobar banner UI.
class DefaultInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  DefaultInfobarBannerInteractionHandler();
  ~DefaultInfobarBannerInteractionHandler() override;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_
