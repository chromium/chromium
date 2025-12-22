// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class Browser;

enum class PageContextWrapperError;

namespace ios::provider {
enum class BWGPageContextComputationState;
enum class BWGPageContextAttachmentState;
}  // namespace ios::provider

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

@class BWGLinkOpeningHandler;
@class BWGPageStateChangeHandler;
@class BWGSessionHandler;
@class GeminiPageContext;
@class GeminiSuggestionHandler;

@protocol BWGGatewayProtocol;

// A browser agent responsible for presenting the BWG overlay and managing
// its protocol handlers.
class BwgBrowserAgent : public BrowserUserData<BwgBrowserAgent>,
                        FullscreenControllerObserver {
 public:
  BwgBrowserAgent(const BwgBrowserAgent&) = delete;
  BwgBrowserAgent& operator=(const BwgBrowserAgent&) = delete;

  ~BwgBrowserAgent() override;

  // Starts the Gemini flow on a given view controller and with an optional
  // given image attachment.
  // TODO(crbug.com/465535924): Have this method handle complete or pending
  // PageContext extraction internally, and show the FRE if needed.
  void StartGeminiFlow(UIViewController* base_view_controller,
                       UIImage* image_attachment = nil);

  // Presents the BWG overlay on a given view controller with a given expected
  // PageContext.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead.
  void PresentBwgOverlay(
      UIViewController* base_view_controller,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

  // Presents the BWG overlay on a given view controller in a pending state
  // with a partial PageContext.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead.
  void PresentPendingBwgOverlay(
      UIViewController* base_view_controller,
      std::unique_ptr<optimization_guide::proto::PageContext> page_context);

  // Updates the page context for the BWG overlay.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead (and let this be handled internally within the
  // browser agent).
  void UpdateBwgOverlayPageContext(
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

 private:
  explicit BwgBrowserAgent(Browser* browser);
  friend class BrowserUserData<BwgBrowserAgent>;

  // Presents the BWG overlay on a given view controller with page context,
  // given specific computation state and optional image attachment (can be
  // nil).
  void PresentBwgOverlayWithState(
      UIViewController* base_view_controller,
      std::unique_ptr<optimization_guide::proto::PageContext>
          page_context_proto,
      ios::provider::BWGPageContextComputationState computation_state,
      UIImage* image_attachment = nil);

  // Fetches the favicon for the page or a default favicon if not available.
  UIImage* FetchPageFavicon();

  // Adjusts the configuration around the Gemini page context based on user
  // prefs.
  void ApplyUserPrefsToPageContext(GeminiPageContext* gemini_page_context);

  // Sets the UI command handlers on the session handler. This cannot be called
  // in the constructor because some objects fail the protocol conformance test
  // at that time.
  void SetSessionCommandHandlers();

  // FullscreenControllerObserver:
  void FullscreenProgressUpdated(FullscreenController* controller,
                                 CGFloat progress) override;

  // The gateway for bridging internal protocols.
  __strong id<BWGGatewayProtocol> bwg_gateway_ = nullptr;

  // Handler for opening links from BWG.
  __strong BWGLinkOpeningHandler* bwg_link_opening_handler_ = nullptr;

  // Handler for PageState changes.
  __strong BWGPageStateChangeHandler* bwg_page_state_change_handler_ = nullptr;

  // Handler for the BWG sessions.
  __strong BWGSessionHandler* bwg_session_handler_ = nullptr;

  // Handler for Gemini suggestion chips.
  __strong GeminiSuggestionHandler* gemini_suggestion_handler_ = nullptr;

  // Reference to fullscreen controller. Used to observe fullscreen progress
  // updates related to the Gemini overlay.
  raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_BROWSER_AGENT_H_
