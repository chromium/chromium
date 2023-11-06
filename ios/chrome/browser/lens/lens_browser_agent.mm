// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/lens_browser_agent.h"

#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Closes the active WebState in `browser` (if non-null).
void CloseActiveWebStateInBrowser(base::WeakPtr<Browser> weak_browser) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  const int tab_close_index = web_state_list->active_index();
  DCHECK_NE(tab_close_index, WebStateList::kInvalidIndex);
  web_state_list->CloseWebStateAt(tab_close_index,
                                  WebStateList::CLOSE_USER_ACTION);
}

}  // namespace

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

  base::WeakPtr<Browser> weak_browser = browser_->AsWeakPtr();
  ProceduralBlock completion = ^{
    CloseActiveWebStateInBrowser(weak_browser);
  };

  id<LensCommands> lens_commands_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), LensCommands);
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:lens_entrypoint.value()
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromLeft
      presentationCompletion:completion];
  [lens_commands_handler openLensInputSelection:command];
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
