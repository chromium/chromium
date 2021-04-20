// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>

#import "ios/web/text_fragments/text_fragments_manager_impl.h"

#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/shared_highlighting/core/common/text_fragments_constants.h"
#import "components/shared_highlighting/core/common/text_fragments_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"

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

namespace web {

TextFragmentsManagerImpl::TextFragmentsManagerImpl(WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);

  if (base::FeatureList::IsEnabled(web::features::kScrollToTextIOS)) {
    const web::WebState::ScriptCommandCallback callback = base::BindRepeating(
        ^(const base::DictionaryValue& message, const GURL& page_url,
          bool interacted, web::WebFrame* sender_frame) {
          if (web_state_ && sender_frame && sender_frame->IsMainFrame()) {
            DidReceiveJavaScriptResponse(message);
          }
        });

    subscription_ =
        web_state_->AddScriptCommandCallback(callback, kScriptCommandPrefix);
  }
}

TextFragmentsManagerImpl::~TextFragmentsManagerImpl() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

// static
void TextFragmentsManagerImpl::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), std::make_unique<TextFragmentsManagerImpl>(web_state));
  }
}

// static
TextFragmentsManagerImpl* TextFragmentsManagerImpl::FromWebState(
    WebState* web_state) {
  return static_cast<TextFragmentsManagerImpl*>(
      TextFragmentsManager::FromWebState(web_state));
}

void TextFragmentsManagerImpl::DidFinishNavigation(
    WebState* web_state,
    NavigationContext* navigation_context) {
  DCHECK(web_state_ == web_state);
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (!item)
    return;
  ProcessTextFragments(navigation_context, item->GetReferrer());
}

void TextFragmentsManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void TextFragmentsManagerImpl::ProcessTextFragments(
    const web::NavigationContext* context,
    const web::Referrer& referrer) {
  DCHECK(web_state_);
  if (!context || !AreTextFragmentsAllowed(context)) {
    return;
  }

  base::Value parsed_fragments = shared_highlighting::ParseTextFragments(
      web_state_->GetLastCommittedURL());

  if (parsed_fragments.type() == base::Value::Type::NONE) {
    return;
  }

  // Log metrics and cache Referrer for UKM logging.
  shared_highlighting::LogTextFragmentSelectorCount(
      parsed_fragments.GetList().size());
  shared_highlighting::LogTextFragmentLinkOpenSource(referrer.url);
  latest_source_id_ = ukm::ConvertToSourceId(context->GetNavigationId(),
                                             ukm::SourceIdType::NAVIGATION_ID);
  latest_referrer_url_ = referrer.url;

  std::string fragment_param;
  base::JSONWriter::Write(parsed_fragments, &fragment_param);

  std::string script;
  if (base::FeatureList::IsEnabled(
          web::features::kIOSSharedHighlightingColorChange)) {
    script = base::ReplaceStringPlaceholders(
        "__gCrWeb.textFragments.handleTextFragments($1, $2, $3, $4)",
        {fragment_param,
         /* scroll = */ "true",
         /* backgroundColor = */
         ToHexStringRGB(shared_highlighting::kFragmentTextBackgroundColorARGB),
         /* foregroundColor = */
         ToHexStringRGB(shared_highlighting::kFragmentTextForegroundColorARGB)},
        /* offsets= */ nil);
  } else {
    script = base::ReplaceStringPlaceholders(
        "__gCrWeb.textFragments.handleTextFragments($1, $2, $3, $4)",
        {fragment_param, /* scroll = */ "true", "null", "null"},
        /* offsets= */ nil);
  }

  web_state_->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

#pragma mark - Private Methods

// Returns false if fragments highlighting is not allowed in the current
// |context|.
bool TextFragmentsManagerImpl::AreTextFragmentsAllowed(
    const web::NavigationContext* context) {
  if (!base::FeatureList::IsEnabled(web::features::kScrollToTextIOS)) {
    return false;
  }

  if (!web_state_ || web_state_->HasOpener()) {
    // TODO(crbug.com/1099268): Loosen this restriction if the opener has the
    // same domain.
    return false;
  }

  return context->HasUserGesture() && !context->IsSameDocument();
}

void TextFragmentsManagerImpl::DidReceiveJavaScriptResponse(
    const base::DictionaryValue& response) {
  const std::string* command = response.FindStringKey("command");
  if (!command || *command != kScriptResponseCommand) {
    return;
  }

  // Log success metrics.
  base::Optional<double> optional_fragment_count =
      response.FindDoublePath("result.fragmentsCount");
  base::Optional<double> optional_success_count =
      response.FindDoublePath("result.successCount");

  // Since the response can't be trusted, don't log metrics if the results look
  // invalid.
  if (!optional_fragment_count ||
      optional_fragment_count.value() > kMaxSelectorCount ||
      optional_fragment_count.value() <= kMinSelectorCount) {
    return;
  }
  if (!optional_success_count ||
      optional_success_count.value() > kMaxSelectorCount ||
      optional_success_count.value() < kMinSelectorCount) {
    return;
  }
  if (optional_success_count.value() > optional_fragment_count.value()) {
    return;
  }

  int fragment_count = static_cast<int>(optional_fragment_count.value());
  int success_count = static_cast<int>(optional_success_count.value());

  shared_highlighting::LogTextFragmentMatchRate(success_count, fragment_count);
  shared_highlighting::LogTextFragmentAmbiguousMatch(
      /*ambiguous_match=*/success_count != fragment_count);

  shared_highlighting::LogLinkOpenedUkmEvent(
      latest_source_id_, latest_referrer_url_,
      /*success=*/success_count == fragment_count);
}

WEB_STATE_USER_DATA_KEY_IMPL(TextFragmentsManager)

}  // namespace web
