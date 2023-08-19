// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_action_util.h"

#import <WebKit/WebKit.h>

namespace web {

NavigationActionInitiationType GetNavigationActionInitiationType(
    WKNavigationAction* action) {
  if (UIAccessibilityIsVoiceOverRunning())
    return GetNavigationActionInitiationTypeWithVoiceOverOn(action.description);
  return GetNavigationActionInitiationTypeWithVoiceOverOff(action.description);
}

// Returns the NavigationAction initiation type based on the string description
// of WKNavigationAction object when voice over is off.
// WKNavigationAction lacks public APIs to access KNavigationAction properties,
// and for that this function parses the description string of the object to get
// information about the action initiator. It uses the SyntheticClickType to
// indentify if there was a user guesture.
// Example description where the syntheticClickType is set:
// "<classname: 0x123124; navigationType = 2; syntheticClickType = 1;
//  position x = 90.20 y = 10.29 request = null; ..>"
// SyntheticClickType still can be 0 for user-initiated actions.
NavigationActionInitiationType
GetNavigationActionInitiationTypeWithVoiceOverOff(
    NSString* action_description) {
  NSRegularExpression* click_type_regex = [NSRegularExpression
      regularExpressionWithPattern:@"\\bsyntheticClickType = ([0-2]);"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  NSTextCheckingResult* click_type_match_result = [click_type_regex
      firstMatchInString:action_description
                 options:0
                   range:NSMakeRange(0, action_description.length)];

  if (![click_type_match_result rangeAtIndex:0].length)
    return NavigationActionInitiationType::kUnknownInitiator;

  NSRange match_range = [click_type_match_result rangeAtIndex:1];
  // SyntheticClickType represents the user action that happened to initiate
  // this navigation values can be {0: NoTap, 1: OneFingerTap, 2:
  // TwoFingerTap}.
  int click_type = [action_description substringWithRange:match_range].intValue;
  return click_type ? NavigationActionInitiationType::kUserInitiated
                    : NavigationActionInitiationType::kUnknownInitiator;
}

// Returns the NavigationAction initiation type based on the string description
// of WKNavigationAction object when voice over is on.
// In the voice over case, SyntheticClickType field is not filled so so this
// function uses the coordinates of the touch on the root view to indentify if
// there was a user guesture.
// Example description where only position is set:
// "<classname: 0x121124; navigationType = 1; syntheticClickType = 0;
//  position x = 90.20 y = 10.29 request = null; ..>"
// Both coordinates still can be 0 for user-initiated actions.
NavigationActionInitiationType GetNavigationActionInitiationTypeWithVoiceOverOn(
    NSString* action_description) {
  NSRegularExpression* position_regex = [NSRegularExpression
      regularExpressionWithPattern:@"\\bposition x = ([0-9]+\\.?[0-9]+) y = "
                                   @"([0-9]+\\.?[0-9]+)\\b"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  NSTextCheckingResult* position_match_result = [position_regex
      firstMatchInString:action_description
                 options:0
                   range:NSMakeRange(0, action_description.length)];

  if (![position_match_result rangeAtIndex:0].length)
    return NavigationActionInitiationType::kUnknownInitiator;

  float position_x =
      [action_description
          substringWithRange:[position_match_result rangeAtIndex:1]]
          .floatValue;
  float position_y =
      [action_description
          substringWithRange:[position_match_result rangeAtIndex:2]]
          .floatValue;
  return (position_y || position_x)
             ? NavigationActionInitiationType::kUserInitiated
             : NavigationActionInitiationType::kUnknownInitiator;
}

}  // namespace web
