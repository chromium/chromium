// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/disabled_sites.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "url/gurl.h"

// Interface encapsulating the properties needed to check the contents of the
// selection and whether or not it is editable.
@protocol EditableTextInput <UITextInput>
- (BOOL)isEditable;
@end

namespace {

// Pattern to identify non-whitespace/punctuation characters. Mirrors the regex
// used in the JS lib to identify non-boundary characters.
NSString* const kNotBoundaryCharPattern = @"[^\\p{P}\\s]";

// Limit the search for a non-boundary char to the first 200 characters,
// to ensure this regex does not have performance impact.
const int kBoundaryCharSearchLimit = 200;

// Corresponds to LinkToTextShouldOfferResult in enums.xml; used to log
// fine-grained behavior of ShouldOffer.
enum class ShouldOfferResult {
  kSuccess = 0,
  kBlockListed = 2,
  kUnableToInvokeJavaScript = 3,
  kSelectionEmpty = 6,
  kUserEditing = 7,
  kTextInputNotFound = 8,
  kPartialSuccess = 9,

  // Deprecated. Do not reuse, change, or remove these values.
  kRejectedInJavaScript = 1,
  kWebLayerTaskTimeout = 4,
  kDispatchedTimeout = 5,

  kMaxValue = kPartialSuccess
};

void LogShouldOfferResult(ShouldOfferResult result) {
  base::UmaHistogramEnumeration("IOS.LinkToText.ShouldOfferResult", result);
}

// Traverse subviews to find the one responsible for the text selection
// behavior (UITextInput).
UIView<EditableTextInput>* FindInput(UIView* root) {
  if ([root conformsToProtocol:@protocol(UITextInput)] &&
      [root respondsToSelector:@selector(isEditable)]) {
    return (UIView<EditableTextInput>*)root;
  }
  for (UIView* view in [root subviews]) {
    auto* maybe_input = FindInput(view);
    if (maybe_input) {
      return maybe_input;
    }
  }
  return nil;
}

}  // namespace

LinkToTextTabHelper::LinkToTextTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  web_state_->AddObserver(this);
}

LinkToTextTabHelper::~LinkToTextTabHelper() {}

bool LinkToTextTabHelper::ShouldOffer() {
  if (!shared_highlighting::ShouldOfferLinkToText(
          web_state_->GetLastCommittedURL())) {
    LogShouldOfferResult(ShouldOfferResult::kBlockListed);
    return false;
  }

  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_state_->ContentIsHTML() || !main_frame) {
    LogShouldOfferResult(ShouldOfferResult::kUnableToInvokeJavaScript);
    return false;
  }

  UIView<EditableTextInput>* textInputView = FindInput(web_state_->GetView());

  if (!textInputView) {
    LogShouldOfferResult(ShouldOfferResult::kTextInputNotFound);
    DUMP_WILL_BE_NOTREACHED();
    return false;
  }

  if ([textInputView isEditable]) {
    LogShouldOfferResult(ShouldOfferResult::kUserEditing);
    return false;
  }

  NSString* selection =
      [textInputView textInRange:[textInputView selectedTextRange]];

  if (!selection) {
    // A bug on older versions can cause selection to be nil. In this case, we
    // offer the feature even though it might just be whitespace.
    LogShouldOfferResult(ShouldOfferResult::kPartialSuccess);
    return true;
  }

  if (IsOnlyBoundaryChars(selection)) {
    LogShouldOfferResult(ShouldOfferResult::kSelectionEmpty);
    return false;
  }

  LogShouldOfferResult(ShouldOfferResult::kSuccess);
  return true;
}

void LinkToTextTabHelper::GetLinkToText(
    base::OnceCallback<void(LinkToTextResponse*)> callback) {
  GetJSFeature()->GetLinkToText(web_state_, std::move(callback));
}

void LinkToTextTabHelper::SetJSFeatureForTesting(
    LinkToTextJavaScriptFeature* js_feature) {
  js_feature_for_testing_ = js_feature;
}

bool LinkToTextTabHelper::IsOnlyBoundaryChars(NSString* str) {
  if (!not_boundary_char_regex_) {
    NSError* error = nil;
    not_boundary_char_regex_ = [NSRegularExpression
        regularExpressionWithPattern:kNotBoundaryCharPattern
                             options:0
                               error:&error];
    if (error) {
      // We should never get an error from compiling the regex, since it's a
      // literal.
      NOTREACHED_IN_MIGRATION();
      return true;
    }
  }
  int max_len = MIN(kBoundaryCharSearchLimit, [str length]);
  NSRange range = [not_boundary_char_regex_
      rangeOfFirstMatchInString:str
                        options:0
                          range:NSMakeRange(0, max_len)];
  return range.location == NSNotFound;
}

LinkToTextJavaScriptFeature* LinkToTextTabHelper::GetJSFeature() {
  return js_feature_for_testing_ ? js_feature_for_testing_.get()
                                 : LinkToTextJavaScriptFeature::GetInstance();
}

void LinkToTextTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  web_state_->RemoveObserver(this);
  web_state_ = nil;

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  web_state->RemoveUserData(UserDataKey());
}

WEB_STATE_USER_DATA_KEY_IMPL(LinkToTextTabHelper)
