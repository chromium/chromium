// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative/declarative_constants.h"

namespace extensions {
namespace declarative_content_constants {

// Signals to which ContentRulesRegistries are registered.
const char kOnPageChanged[] = "declarativeContent.onPageChanged";

// Keys of dictionaries.
const char kAllFrames[] = "allFrames";
const char kCss[] = "css";
const char kInstanceType[] = "instanceType";
const char kIsBookmarked[] = "isBookmarked";
const char kJs[] = "js";
const char kMatchAboutBlank[] = "matchAboutBlank";
const char kPageUrl[] = "pageUrl";

// Values of dictionaries, in particular instance types
const char kPageStateMatcherType[] = "declarativeContent.PageStateMatcher";
const char kShowAction[] = "declarativeContent.ShowAction";
const char kRequestContentScript[] = "declarativeContent.RequestContentScript";
const char kSetIcon[] = "declarativeContent.SetIcon";

const char kLegacyShowAction[] = "declarativeContent.ShowPageAction";

}  // namespace declarative_content_constants
}  // namespace extensions
