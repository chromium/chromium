// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_IDENTIFIER_CONTENT_SUGGESTION_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_IDENTIFIER_CONTENT_SUGGESTION_IDENTIFIER_H_

#import <Foundation/Foundation.h>

#include <string>

@class ContentSuggestionsSectionInformation;

// An class identifying a suggestion.
@interface ContentSuggestionIdentifier : NSObject

@property(nonatomic, strong) ContentSuggestionsSectionInformation* sectionInfo;
// Must be unique inside a section.
@property(nonatomic, assign) std::string IDInSection;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_IDENTIFIER_CONTENT_SUGGESTION_IDENTIFIER_H_
