// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/model/lens_browser_agent.h"

#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

LensBrowserAgent::LensBrowserAgent(Browser* browser) : browser_(browser) {
  browser->AddObserver(this);
}

LensBrowserAgent::~LensBrowserAgent() = default;

#pragma mark - Public

bool LensBrowserAgent::CanGoBackToLensViewFinder() const {
  return CurrentResultsEntrypoint() == LensEntrypoint::NewTabPage;
}

void LensBrowserAgent::GoBackToLensViewFinder() const {
  DCHECK(browser_);

  std::optional<LensEntrypoint> lens_entrypoint = CurrentResultsEntrypoint();
  if (!lens_entrypoint) {
    return;
  }

  id<LensCommands> lens_commands_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), LensCommands);
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:lens_entrypoint.value()
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromLeft
      presentationCompletion:nil];
  [lens_commands_handler openLensInputSelection:command];
}

#pragma mark - Private

std::optional<LensEntrypoint> LensBrowserAgent::CurrentResultsEntrypoint()
    const {
  DCHECK(browser_);

  if (!ios::provider::IsLensSupported()) {
    return std::nullopt;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebState* web_state = web_state_list->GetActiveWebState();
  // Return null optional if there is no active WebState.
  if (!web_state) {
    return std::nullopt;
  }

  // Lens camera is unsupported if the default search engine is not Google.
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  const TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(url_service);

  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  if (!default_url ||
      default_url->GetEngineType(url_service->search_terms_data()) !=
          SEARCH_ENGINE_GOOGLE) {
    return std::nullopt;
  }

  // The URL must be a valid Lens Web results URL from the camera with a defined
  // entry point.
  std::optional<LensEntrypoint> entry_point =
      ios::provider::GetLensEntryPointFromURL(web_state->GetVisibleURL());
  if (!entry_point) {
    return std::nullopt;
  }

  // Return a null optional if the entrypoint is not an enabled camera
  // experience.
  switch (entry_point.value()) {
    case LensEntrypoint::Keyboard:
    case LensEntrypoint::NewTabPage:
    case LensEntrypoint::HomeScreenWidget:
      if (base::FeatureList::IsEnabled(kDisableLensCamera)) {
        return std::nullopt;
      }
      return entry_point;
    default:
      return std::nullopt;
  }
}

#pragma mark - BrowserObserver

void LensBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

BROWSER_USER_DATA_KEY_IMPL(LensBrowserAgent)
