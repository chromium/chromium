// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_UTIL_H_

#import <Foundation/Foundation.h>

@class AutofillAiSourceGroup;

namespace autofill {
class EntityInstance;
}  // namespace autofill

// Returns YES if the given `entity` has personal context payload containing at
// least one source with a valid URL.
BOOL EntityHasValidSources(const autofill::EntityInstance& entity);

// Extracts and groups source items from the given `entity`.
// Sources are grouped by service (Gmail first, then Google Photos) and only
// sources with valid URLs are included. Returns an empty array if no valid
// sources are found.
NSArray<AutofillAiSourceGroup*>* ExtractSourcesFromEntity(
    const autofill::EntityInstance& entity);

// Formats the subtitle for the sources bottom sheet header (e.g.,
// "Order · AN-147338") from the `entity` type name and `suggestion_value`.
NSString* SourcesHeaderSubtitle(const autofill::EntityInstance& entity,
                                NSString* suggestion_value);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_UTIL_H_
