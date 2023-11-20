// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"

namespace {

// The scheme for google chrome action URLs.
NSString* const kGoogleChromeActionScheme = @"googlechromeaction";

// The host for google chrome action Lens URLs.
NSString* const kLensHost = @"lens";

// The path for the web search bar entry point. The logging path is WebSearchBar
// while the url path should be web-search-box to match convention with other
// entry points.
NSString* const kWebSearchBarEntryPointPath = @"/web-search-box";

// The path for the search translate box entry point.
NSString* const kTranslateOneboxEntryPointPath = @"/translate-onebox";

// The path for the web image search bar entry point.
NSString* const kWebImagesSearchBarEntryPointPath = @"/web-images-search-box";

}  // namespace

LensTabHelper::LensTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

LensTabHelper::~LensTabHelper() = default;

void LensTabHelper::SetLensCommandsHandler(id<LensCommands> commands_handler) {
  commands_handler_ = commands_handler;
}

std::optional<LensEntrypoint>
LensTabHelper::EntryPointForGoogleChromeActionURLPath(NSString* path) {
  if ([path caseInsensitiveCompare:kWebSearchBarEntryPointPath] ==
      NSOrderedSame) {
    return LensEntrypoint::WebSearchBar;
  } else if ([path caseInsensitiveCompare:kTranslateOneboxEntryPointPath] ==
             NSOrderedSame) {
    return LensEntrypoint::TranslateOnebox;
  } else if ([path caseInsensitiveCompare:kWebImagesSearchBarEntryPointPath] ==
             NSOrderedSame) {
    return LensEntrypoint::WebImagesSearchBar;
  }
  return std::nullopt;
}

void LensTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  if (request_info.target_frame_is_main &&
      [request.URL.scheme isEqualToString:kGoogleChromeActionScheme] &&
      [request.URL.host isEqualToString:kLensHost]) {
    std::optional<LensEntrypoint> entry_point =
        EntryPointForGoogleChromeActionURLPath(request.URL.path);
    if (entry_point) {
      OpenLensInputSelection(entry_point.value());
      std::move(callback).Run(PolicyDecision::Cancel());
      return;
    }
  }

  std::move(callback).Run(PolicyDecision::Allow());
}

void LensTabHelper::OpenLensInputSelection(LensEntrypoint entry_point) {
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:entry_point
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [commands_handler_ openLensInputSelection:command];
}

WEB_STATE_USER_DATA_KEY_IMPL(LensTabHelper)
