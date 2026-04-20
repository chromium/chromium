// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

// Enum representing the possible status codes returned by the
// text-fragments-polyfill library. To be kept in sync with the
// `GenerateFragmentStatus` enum in that library.
//
// These values are serialized as numbers in the `status` field of the
// dictionary returned by `GetTextFragment`.
enum class TextFragmentGenerationStatus {
  kSuccess = 0,
  kInvalidSelection = 1,
  kAmbiguous = 2,
  kTimeout = 3,
  kExecutionFailed = 4,
};

// Struct representing a generated text fragment.
struct SendTabToSelfTextFragment {
  SendTabToSelfTextFragment();
  SendTabToSelfTextFragment(const SendTabToSelfTextFragment&);
  SendTabToSelfTextFragment(SendTabToSelfTextFragment&&);
  SendTabToSelfTextFragment& operator=(const SendTabToSelfTextFragment&);
  SendTabToSelfTextFragment& operator=(SendTabToSelfTextFragment&&);
  ~SendTabToSelfTextFragment();

  TextFragmentGenerationStatus status;
  std::string text_start;
  std::string text_end;
  std::string prefix;
  std::string suffix;
};

// Feature that handles JavaScript-side logic for Send Tab To Self, such as
// generating text fragments for the viewport center.
class SendTabToSelfTextFragmentSelectorGenerator
    : public web::JavaScriptFeature {
 public:
  static SendTabToSelfTextFragmentSelectorGenerator* GetInstance();

  // Attempts to generate a text fragment for the viewport center.
  // The `callback` will be called with the result of the JS execution.
  void GetTextFragment(
      web::WebState* web_state,
      base::OnceCallback<void(std::optional<SendTabToSelfTextFragment>)>
          callback);

  // Attempts to scroll the page to the given text fragment.
  void ScrollToTextFragment(web::WebState* web_state,
                            std::string_view text_fragment);

 private:
  friend class base::NoDestructor<SendTabToSelfTextFragmentSelectorGenerator>;

  SendTabToSelfTextFragmentSelectorGenerator();
  ~SendTabToSelfTextFragmentSelectorGenerator() override;

  SendTabToSelfTextFragmentSelectorGenerator(
      const SendTabToSelfTextFragmentSelectorGenerator&) = delete;
  SendTabToSelfTextFragmentSelectorGenerator& operator=(
      const SendTabToSelfTextFragmentSelectorGenerator&) = delete;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
