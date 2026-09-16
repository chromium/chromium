// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;
class Browser;

namespace autofill {
struct Suggestion;
}

// Reasons why the AtMemory UI failed to open when the user tapped the AtMemory
// button. `kNone` means the UI was opened. These values are persisted to logs;
// entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(AtMemoryFailedToOpenReason)
enum class AtMemoryFailedToOpenReason {
  // All preconditions were met and the AtMemory UI was opened.
  kNone = 0,
  // There is no browser or no web state list attached to it.
  kNoBrowser = 1,
  // There is no active web state.
  kNoActiveWebState = 2,
  // The active web state has no autofill client.
  kNoAutofillClient = 3,
  // The autofill client has no AtMemory manager.
  kNoAtMemoryManager = 4,
  // The autofill client has no autofill manager for the primary main frame.
  kNoAutofillManager = 5,
  // There is no focused field to fill.
  kNoFocusedField = 6,
  // The AtMemory UI is already open.
  kAlreadyOpen = 7,
  kMaxValue = kAlreadyOpen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AtMemoryFailedToOpenReason)

// Returns the reason preventing the AtMemory UI from being shown for `browser`,
// or `AtMemoryFailedToOpenReason::kNone` if all the objects the AtMemory UI
// depends on are available. Reasons that don't depend on `browser`, such as
// `kNoFocusedField`, are never returned.
AtMemoryFailedToOpenReason GetAtMemoryFailedToOpenReason(Browser* browser);

// Returns the granular fill title for `suggestion`.
NSString* GetAtMemoryGranularFillTitle(const autofill::Suggestion& suggestion);

// Returns an array of AtMemoryGranularFillItem objects created from
// `suggestion`.
NSArray<AtMemoryGranularFillItem*>* AtMemoryGranularFillItemsForSuggestion(
    const autofill::Suggestion& suggestion);

// Returns the accessibility identifier for the granular fill cell
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillCellAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the granular fill attribute label
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillAttributeLabelAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the granular fill chip button
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillChipButtonAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the search result cell
// corresponding to `title`.
NSString* GetAtMemorySearchResultCellAccessibilityIdentifier(NSString* title);

// Returns the accessibility identifier for the search result's info button
// corresponding to `title`.
NSString* GetAtMemorySearchResultInfoButtonAccessibilityIdentifier(
    NSString* title);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
