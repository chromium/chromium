// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/lens_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

LensBrowserAgent::LensBrowserAgent(Browser* browser) : browser_(browser) {
  browser->AddObserver(this);
}

LensBrowserAgent::~LensBrowserAgent() = default;

#pragma mark - Public

bool LensBrowserAgent::CanGoBackToLensViewFinder() const {
  return CurrentResultsEntrypoint().has_value();
}

void LensBrowserAgent::GoBackToLensViewFinder() const {
  DCHECK(browser_);

  absl::optional<LensEntrypoint> lens_entrypoint = CurrentResultsEntrypoint();
  if (!lens_entrypoint) {
    return;
  }

  id<LensCommands> lens_commands_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), LensCommands);
  [lens_commands_handler
      openInputSelectionForEntrypoint:lens_entrypoint.value()];

  // Since the user is returning to the camera experience that opened the
  // tab in the first place, close the tab.
  WebStateList* web_state_list = browser_->GetWebStateList();
  const int index = web_state_list->active_index();
  DCHECK_NE(index, WebStateList::kInvalidIndex);
  web_state_list->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

#pragma mark - Private

absl::optional<LensEntrypoint> LensBrowserAgent::CurrentResultsEntrypoint()
    const {
  DCHECK(browser_);

  if (!ios::provider::IsLensSupported()) {
    return absl::nullopt;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebState* web_state = web_state_list->GetActiveWebState();
  // Return null optional if there is no active WebState.
  if (!web_state) {
    return absl::nullopt;
  }

  // Lens camera is unsupported if the default search engine is not Google.
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  const TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  DCHECK(url_service);

  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  if (!default_url ||
      default_url->GetEngineType(url_service->search_terms_data()) !=
          SEARCH_ENGINE_GOOGLE) {
    return absl::nullopt;
  }

  // The URL must be a valid Lens Web results URL from the camera with a defined
  // entry point.
  absl::optional<LensEntrypoint> entry_point =
      ios::provider::GetLensEntryPointFromURL(web_state->GetVisibleURL());
  if (!entry_point) {
    return absl::nullopt;
  }

  // Return a null optional if the entrypoint is not an enabled camera
  // experience.
  switch (entry_point.value()) {
    case LensEntrypoint::Keyboard:
      if (!base::FeatureList::IsEnabled(kEnableLensInKeyboard)) {
        return absl::nullopt;
      }
      break;
    case LensEntrypoint::NewTabPage:
      if (!base::FeatureList::IsEnabled(kEnableLensInNTP)) {
        return absl::nullopt;
      }
      break;
    case LensEntrypoint::HomeScreenWidget:
      if (!base::FeatureList::IsEnabled(kEnableLensInHomeScreenWidget)) {
        return absl::nullopt;
      }
      break;
    default:
      return absl::nullopt;
  }

  return entry_point;
}

#pragma mark - BrowserObserver

void LensBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

BROWSER_USER_DATA_KEY_IMPL(LensBrowserAgent)
