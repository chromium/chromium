// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/files/file_path.h"
#import "base/values.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "services/metrics/public/cpp/ukm_source_id.h"

@protocol MiniMapCommands;
@protocol UnitConversionCommands;

// Wraps information to add/show to/in a context menu
@interface ElementsToAddToContextMenu : NSObject

// The title of the context menu. Can be nil.
@property(nonatomic, copy) NSString* title;

// List of elements to add to a context menu. Can be nil.
@property(nonatomic, copy) NSMutableArray<UIMenuElement*>* elements;

@end

namespace web {
class WebState;
}  // namespace web

namespace ios {
namespace provider {

// Returns the elements to add to the context menu, with their title. If no
// elements needs to be added, returns nil.
ElementsToAddToContextMenu* GetContextMenuElementsToAdd(
    web::WebState* web_state,
    web::ContextMenuParams params,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler,
    id<UnitConversionCommands> unit_conversion_handler);

// Returns set of `NSTextCheckingType` representing the intent types that
// can be handled by the provider, for the given `web_state`.
NSTextCheckingType GetHandledIntentTypes(web::WebState* web_state);

// Returns set of `NSTextCheckingType` representing the intent types that can be
// handled by the provider in case of one tap experience, for the given
// `web_state`.
NSTextCheckingType GetHandledIntentTypesForOneTap(web::WebState* web_state);

// Executes 1-tap action for the given `match`'s type and returns YES. Returns
// NO if no direct 1-tap action is defined.
BOOL HandleIntentTypesForOneTap(
    web::WebState* web_state,
    NSTextCheckingResult* match,
    NSString* text,
    CGPoint location,
    UIViewController* presenting_view_controller,
    id<MiniMapCommands> mini_map_handler,
    id<UnitConversionCommands> unit_conversion_handler);

// Returns a full set of intents of `handled_types`, located inside `text`. The
// `model_path` for the give web state should be passed in if a detection by
// model is required. (Note that some flags might still not allow it.)
std::optional<std::vector<web::TextAnnotation>> ExtractTextAnnotationFromText(
    const base::Value::Dict& metadata,
    const std::string& text,
    NSTextCheckingType handled_types,
    ukm::SourceId source_id,
    const base::FilePath& model_path);

// Returns the context menu title with styling.
NSString* StyledContextMenuStringForString(NSString* string);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
