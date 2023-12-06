// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_

#include <map>

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mock implementation of InfobarInteractionHandler for use in tests.
class MockInfobarInteractionHandler : public InfobarInteractionHandler {
 public:
  // Mock handler object used by the builder
  class Handler : public InfobarInteractionHandler::Handler {
   public:
    Handler();
    ~Handler() override;

    MOCK_METHOD0(CreateInstaller,
                 std::unique_ptr<OverlayRequestCallbackInstaller>(void));
    MOCK_METHOD2(InfobarVisibilityChanged,
                 void(InfoBarIOS* infobar, bool visible));
  };

  // Helper object that builds InfobarInteractionHandlers with mock handlers.
  class Builder {
   public:
    explicit Builder(InfobarType infobar_type);
    ~Builder();

    // Constructs an InfobarInteractionHandler using mock handlers.  Calling
    // this function also populates `mock_handlers_`.  Must only be called once
    // per Builder.
    std::unique_ptr<InfobarInteractionHandler> Build();

    // Returns the mock handler for `overlay_type` used to build the
    // InfobarInteractionHandler.  Returns null before Build() is called.
    Handler* mock_handler(InfobarOverlayType overlay_type) {
      return mock_handlers_[overlay_type];
    }

   private:
    InfobarType infobar_type_;
    bool has_built_ = false;
    std::map<InfobarOverlayType, Handler*> mock_handlers_;
  };
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_
