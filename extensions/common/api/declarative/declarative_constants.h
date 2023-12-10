// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the declarativeContent API.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_CONSTANTS_H_

namespace extensions::declarative_content_constants {

// Signals to which ContentRulesRegistries are registered.
extern const char kOnPageChanged[];

// Keys of dictionaries.
extern const char kAllFrames[];
extern const char kCss[];
extern const char kInstanceType[];
extern const char kIsBookmarked[];
extern const char kJs[];
extern const char kMatchAboutBlank[];
extern const char kPageUrl[];

// Values of dictionaries, in particular instance types
extern const char kPageStateMatcherType[];
extern const char kShowAction[];
extern const char kRequestContentScript[];
extern const char kSetIcon[];

// The old ShowAction instance type.
extern const char kLegacyShowAction[];

// Describes the injected action type. Used for logging when an action is
// created. Entries should not be renumbered and numeric values should never be
// reused.
enum class ContentActionType {
  kShowAction = 0,
  kSetIcon = 1,
  kRequestContentScript = 2,
  kMaxValue = kRequestContentScript,
};

}  // namespace extensions::declarative_content_constants

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_CONSTANTS_H_
