// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <iomanip>

#import "ios/web/text_fragments/text_fragments_manager_impl.h"

#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/shared_highlighting/core/common/fragment_directives_constants.h"
#import "components/shared_highlighting/core/common/fragment_directives_utils.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"

namespace {
// Returns a rgb hexadecimal color, suitable for processing in JavaScript
std::string ToHexStringRGB(int color) {
  std::stringstream sstream;
  sstream << std::setfill('0') << std::setw(6) << std::hex
          << (color & 0x00FFFFFF);
  return sstream.str();
}

}  // namespace

namespace web {

TextFragmentsManagerImpl::TextFragmentsManagerImpl(WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  web::WebFramesManager* web_frames_manager =
      GetJSFeature()->GetWebFramesManager(web_state);
  web_frames_manager->AddObserver(this);
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

void TextFragmentsManagerImpl::RemoveHighlights() {
  // Remove the fragments that are visible on the page and update the URL.
  GetJSFeature()->RemoveHighlights(
      web_state_, shared_highlighting::RemoveFragmentSelectorDirectives(
                      web_state_->GetLastCommittedURL()));
}

void TextFragmentsManagerImpl::RegisterDelegate(
    id<TextFragmentsDelegate> delegate) {
  delegate_ = delegate;
}

void TextFragmentsManagerImpl::OnProcessingComplete(int success_count,
                                                    int fragment_count) {
  shared_highlighting::LogTextFragmentMatchRate(success_count, fragment_count);
  shared_highlighting::LogTextFragmentAmbiguousMatch(
      /*ambiguous_match=*/success_count != fragment_count);

  shared_highlighting::LogLinkOpenedUkmEvent(
      latest_source_id_, latest_referrer_url_,
      /*success=*/success_count == fragment_count);
}

void TextFragmentsManagerImpl::OnClick() {
  if (delegate_) {
    [delegate_ userTappedTextFragmentInWebState:web_state_];
  } else {
    RemoveHighlights();
  }
}

void TextFragmentsManagerImpl::OnClickWithSender(
    CGRect rect,
    NSString* text,
    std::vector<shared_highlighting::TextFragment> fragments) {
  if (delegate_) {
    [delegate_ userTappedTextFragmentInWebState:web_state_
                                     withSender:rect
                                       withText:text
                                  withFragments:std::move(fragments)];
  }
}

void TextFragmentsManagerImpl::WebFrameBecameAvailable(
    WebFramesManager* web_frames_manager,
    WebFrame* web_frame) {
  if (web_frame->IsMainFrame() && deferred_processing_params_) {
    DoHighlight();
  }
}

void TextFragmentsManagerImpl::DidFinishNavigation(
    WebState* web_state,
    NavigationContext* navigation_context) {
  DCHECK(web_state_ == web_state);
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (!item)
    return;
  auto params = ProcessTextFragments(navigation_context, item->GetReferrer());
  if (!params) {
    // null params indicate that no further processing should happen on this
    // navigation
    deferred_processing_params_ = {};
    return;
  }
  deferred_processing_params_ = std::move(params);
  if (GetJSFeature()->GetWebFramesManager(web_state_)->GetMainWebFrame()) {
    DoHighlight();
  }
}

void TextFragmentsManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - Private Methods

std::optional<TextFragmentsManagerImpl::TextFragmentProcessingParams>
TextFragmentsManagerImpl::ProcessTextFragments(
    const web::NavigationContext* context,
    const web::Referrer& referrer) {
  DCHECK(web_state_);
  if (!context || !AreTextFragmentsAllowed(context)) {
    return {};
  }

  base::Value parsed_fragments = shared_highlighting::ParseTextFragments(
      web_state_->GetLastCommittedURL());

  if (parsed_fragments.type() == base::Value::Type::NONE) {
    return {};
  }

  // Log metrics and cache Referrer for UKM logging.
  shared_highlighting::LogTextFragmentSelectorCount(
      parsed_fragments.GetList().size());
  shared_highlighting::LogTextFragmentLinkOpenSource(referrer.url);
  latest_source_id_ = ukm::ConvertToSourceId(context->GetNavigationId(),
                                             ukm::SourceIdType::NAVIGATION_ID);
  latest_referrer_url_ = referrer.url;

  std::string bg_color;
  std::string fg_color;

  if (base::FeatureList::IsEnabled(
          web::features::kIOSSharedHighlightingColorChange)) {
    bg_color =
        ToHexStringRGB(shared_highlighting::kFragmentTextBackgroundColorARGB);
    fg_color =
        ToHexStringRGB(shared_highlighting::kFragmentTextForegroundColorARGB);
  }

  return std::optional<TextFragmentProcessingParams>(
      {std::move(parsed_fragments), bg_color, fg_color});
}

void TextFragmentsManagerImpl::DoHighlight() {
  GetJSFeature()->ProcessTextFragments(
      web_state_, std::move(deferred_processing_params_->parsed_fragments),
      deferred_processing_params_->bg_color,
      deferred_processing_params_->fg_color);
  deferred_processing_params_ = {};
}

// Returns false if fragments highlighting is not allowed in the current
// `context`.
bool TextFragmentsManagerImpl::AreTextFragmentsAllowed(
    const web::NavigationContext* context) {
  if (!web_state_ || web_state_->HasOpener()) {
    // TODO(crbug.com/40137397): Loosen this restriction if the opener has the
    // same domain.
    return false;
  }

  return context->HasUserGesture() && !context->IsSameDocument();
}

TextFragmentsJavaScriptFeature* TextFragmentsManagerImpl::GetJSFeature() {
  return js_feature_for_testing_
             ? js_feature_for_testing_.get()
             : TextFragmentsJavaScriptFeature::GetInstance();
}

void TextFragmentsManagerImpl::SetJSFeatureForTesting(
    TextFragmentsJavaScriptFeature* feature) {
  js_feature_for_testing_ = feature;
}

WEB_STATE_USER_DATA_KEY_IMPL(TextFragmentsManager)

}  // namespace web
