// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"

namespace ios {
namespace provider {

ElementsToAddToContextMenu* GetContextMenuElementsToAdd(
    ChromeBrowserState* browser_state,
    web::WebState* web_state,
    web::ContextMenuParams params,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler) {
  return nil;
}

NSTextCheckingType GetHandledIntentTypes(web::WebState* web_state) {
  return 0;
}

NSTextCheckingType GetHandledIntentTypesForOneTap(web::WebState* web_state) {
  return 0;
}

BOOL HandleIntentTypesForOneTap(web::WebState* web_state,
                                NSTextCheckingResult* match,
                                NSString* text,
                                UIViewController* presenting_view_controller,
                                id<MiniMapCommands> mini_map_handler) {
  return NO;
}

NSArray<CRWContextMenuItem*>* GetContextMenuElementsToAdd(
    web::WebState* web_state,
    NSTextCheckingResult* match,
    NSString* text,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler) {
  return @[];
}

NSArray<CRWContextMenuItem*>* GetContextMenuElementsToAdd(
    web::WebState* web_state,
    NSTextCheckingResult* match,
    NSString* text,
    CGPoint location,
    UIViewController* presenting_view_controller) {
  return @[];
}

absl::optional<base::Value> ExtractDataElementsFromText(
    const std::string& text,
    NSTextCheckingType handled_types,
    const base::FilePath& model_path) {
  return absl::nullopt;
}

}  // namespace provider
}  // namespace ios
