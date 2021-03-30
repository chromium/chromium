// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>

#import "ios/web/text_fragments/crw_text_fragments_handler.h"

#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/shared_highlighting/core/common/text_fragments_constants.h"
#import "components/shared_highlighting/core/common/text_fragments_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#import "services/metrics/public/cpp/ukm_source_id.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kScriptCommandPrefix[] = "textFragments";
const char kScriptResponseCommand[] = "textFragments.response";

const double kMinSelectorCount = 0.0;
const double kMaxSelectorCount = 200.0;

// Returns a rgb hexadecimal color, suitable for processing in JavaScript
std::string ToHexStringRGB(int color) {
  std::stringstream sstream;
  sstream << "'" << std::setfill('0') << std::setw(6) << std::hex
          << (color & 0x00FFFFFF) << "'";
  return sstream.str();
}

}  // namespace

@interface CRWTextFragmentsHandler () {
  base::CallbackListSubscription _subscription;
}

@property(nonatomic, weak) id<CRWWebViewHandlerDelegate> delegate;

// Returns the WebStateImpl from self.delegate.
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;

// Cached value of the source ID representing the last navigation to have text
// fragments.
@property(nonatomic, assign) ukm::SourceId latestSourceId;

// Cached value of the latest referrer's URL to have triggered a navigation
// with text fragments.
@property(nonatomic, assign) GURL latestReferrerURL;

@end

@implementation CRWTextFragmentsHandler

- (instancetype)initWithDelegate:(id<CRWWebViewHandlerDelegate>)delegate {
  DCHECK(delegate);
  if (self = [super init]) {
    _delegate = delegate;

    if (base::FeatureList::IsEnabled(web::features::kScrollToTextIOS) &&
        self.webStateImpl) {
      __weak __typeof(self) weakSelf = self;
      const web::WebState::ScriptCommandCallback callback = base::BindRepeating(
          ^(const base::DictionaryValue& message, const GURL& page_url,
            bool interacted, web::WebFrame* sender_frame) {
            if (sender_frame && sender_frame->IsMainFrame()) {
              [weakSelf didReceiveJavaScriptResponse:message];
            }
          });

      _subscription = self.webStateImpl->AddScriptCommandCallback(
          callback, kScriptCommandPrefix);
    }
  }

  return self;
}

- (void)processTextFragmentsWithContext:(web::NavigationContext*)context
                               referrer:(web::Referrer)referrer {
  if (!context || ![self areTextFragmentsAllowedInContext:context]) {
    return;
  }

  base::Value parsedFragments = shared_highlighting::ParseTextFragments(
      self.webStateImpl->GetLastCommittedURL());

  if (parsedFragments.type() == base::Value::Type::NONE) {
    return;
  }

  // Log metrics and cache Referrer for UKM logging.
  shared_highlighting::LogTextFragmentSelectorCount(
      parsedFragments.GetList().size());
  shared_highlighting::LogTextFragmentLinkOpenSource(referrer.url);
  self.latestSourceId = ukm::ConvertToSourceId(
      context->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  self.latestReferrerURL = referrer.url;

  std::string fragmentParam;
  base::JSONWriter::Write(parsedFragments, &fragmentParam);

  std::string script;
  if (base::FeatureList::IsEnabled(
          web::features::kIOSSharedHighlightingColorChange)) {
    script = base::ReplaceStringPlaceholders(
        "__gCrWeb.textFragments.handleTextFragments($1, $2, $3, $4)",
        {fragmentParam,
         /* scroll = */ "true",
         /* backgroundColor = */
         ToHexStringRGB(shared_highlighting::kFragmentTextBackgroundColorARGB),
         /* foregroundColor = */
         ToHexStringRGB(shared_highlighting::kFragmentTextForegroundColorARGB)},
        /* offsets= */ nil);
  } else {
    script = base::ReplaceStringPlaceholders(
        "__gCrWeb.textFragments.handleTextFragments($1, $2, $3, $4)",
        {fragmentParam, /* scroll = */ "true", "null", "null"},
        /* offsets= */ nil);
  }

  self.webStateImpl->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

#pragma mark - Private Methods

// Returns NO if fragments highlighting is not allowed in the current |context|.
- (BOOL)areTextFragmentsAllowedInContext:(web::NavigationContext*)context {
  if (!base::FeatureList::IsEnabled(web::features::kScrollToTextIOS)) {
    return NO;
  }

  if (self.isBeingDestroyed) {
    return NO;
  }

  // If the current instance isn't being destroyed, then it must be able to get
  // a valid WebState.
  DCHECK(self.webStateImpl);

  if (self.webStateImpl->HasOpener()) {
    // TODO(crbug.com/1099268): Loosen this restriction if the opener has the
    // same domain.
    return NO;
  }

  return context->HasUserGesture() && !context->IsSameDocument();
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForWebViewHandler:self];
}

- (void)didReceiveJavaScriptResponse:(const base::DictionaryValue&)response {
  const std::string* command = response.FindStringKey("command");
  if (!command || *command != kScriptResponseCommand) {
    return;
  }

  // Log success metrics.
  base::Optional<double> optionalFragmentCount =
      response.FindDoublePath("result.fragmentsCount");
  base::Optional<double> optionalSuccessCount =
      response.FindDoublePath("result.successCount");

  // Since the response can't be trusted, don't log metrics if the results look
  // invalid.
  if (!optionalFragmentCount ||
      optionalFragmentCount.value() > kMaxSelectorCount ||
      optionalFragmentCount.value() <= kMinSelectorCount) {
    return;
  }
  if (!optionalSuccessCount ||
      optionalSuccessCount.value() > kMaxSelectorCount ||
      optionalSuccessCount.value() < kMinSelectorCount) {
    return;
  }
  if (optionalSuccessCount.value() > optionalFragmentCount.value()) {
    return;
  }

  int fragmentCount = static_cast<int>(optionalFragmentCount.value());
  int successCount = static_cast<int>(optionalSuccessCount.value());

  shared_highlighting::LogTextFragmentMatchRate(successCount, fragmentCount);
  shared_highlighting::LogTextFragmentAmbiguousMatch(
      /*ambiguous_match=*/successCount != fragmentCount);

  shared_highlighting::LogLinkOpenedUkmEvent(
      self.latestSourceId, self.latestReferrerURL,
      /*success=*/successCount == fragmentCount);
}

@end
