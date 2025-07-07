// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_utils.h"

#import "base/check.h"
#import "base/notreached.h"

namespace {

// Returns the mapped string to the given IOSOmniboxAssistiveKey.
NSString* AssistiveKeyEnumValueToNSString(IOSOmniboxAssistiveKey assistiveKey) {
  switch (assistiveKey) {
    case IOSOmniboxAssistiveKey::kAssistiveKeyColon:
      return @":";
    case IOSOmniboxAssistiveKey::kAssistiveKeyDash:
      return @"-";
    case IOSOmniboxAssistiveKey::kAssistiveKeySlash:
      return @"/";
    case IOSOmniboxAssistiveKey::kAssistiveKeyDotCom:
      return @".com";
  }

  NOTREACHED();
}

}  // namespace

IOSOmniboxAssistiveKey AssistiveKeyStringToEnumValue(NSString* assistiveKey) {
  // Ensure that the given string is valid.
  DCHECK([AssistiveKeys() containsObject:assistiveKey]);
  if ([assistiveKey
          isEqualToString:AssistiveKeyEnumValueToNSString(
                              IOSOmniboxAssistiveKey::kAssistiveKeyColon)]) {
    return IOSOmniboxAssistiveKey::kAssistiveKeyColon;
  }
  if ([assistiveKey
          isEqualToString:AssistiveKeyEnumValueToNSString(
                              IOSOmniboxAssistiveKey::kAssistiveKeyDash)]) {
    return IOSOmniboxAssistiveKey::kAssistiveKeyDash;
  }
  if ([assistiveKey
          isEqualToString:AssistiveKeyEnumValueToNSString(
                              IOSOmniboxAssistiveKey::kAssistiveKeySlash)]) {
    return IOSOmniboxAssistiveKey::kAssistiveKeySlash;
  }

  DCHECK([assistiveKey
      isEqualToString:AssistiveKeyEnumValueToNSString(
                          IOSOmniboxAssistiveKey::kAssistiveKeyDotCom)]);

  return IOSOmniboxAssistiveKey::kAssistiveKeyDotCom;
}

NSArray<NSString*>* AssistiveKeys() {
  return @[
    AssistiveKeyEnumValueToNSString(IOSOmniboxAssistiveKey::kAssistiveKeyColon),
    AssistiveKeyEnumValueToNSString(IOSOmniboxAssistiveKey::kAssistiveKeyDash),
    AssistiveKeyEnumValueToNSString(IOSOmniboxAssistiveKey::kAssistiveKeySlash),
    AssistiveKeyEnumValueToNSString(IOSOmniboxAssistiveKey::kAssistiveKeyDotCom)
  ];
}
