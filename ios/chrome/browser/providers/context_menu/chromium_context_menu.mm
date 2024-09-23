// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"

namespace ios::provider {

ElementsToAddToContextMenu* GetContextMenuElementsToAdd(
    web::WebState* web_state,
    web::ContextMenuParams params,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler,
    id<UnitConversionCommands> unit_conversion_handler) {
  return nil;
}

NSTextCheckingType GetHandledIntentTypes(web::WebState* web_state) {
  return 0;
}

NSTextCheckingType GetHandledIntentTypesForOneTap(web::WebState* web_state) {
  return 0;
}

BOOL HandleIntentTypesForOneTap(
    web::WebState* web_state,
    NSTextCheckingResult* match,
    NSString* text,
    CGPoint location,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler,
    id<UnitConversionCommands> unit_conversion_handler) {
  return NO;
}

std::optional<std::vector<web::TextAnnotation>> ExtractTextAnnotationFromText(
    const base::Value::Dict& metadata,
    const std::string& text,
    NSTextCheckingType handled_types,
    ukm::SourceId source_id,
    const base::FilePath& model_path) {
  return std::nullopt;
}

NSString* StyledContextMenuStringForString(NSString* string) {
  return string;
}

}  // namespace ios::provider
