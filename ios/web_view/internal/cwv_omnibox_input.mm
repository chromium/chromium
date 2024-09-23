// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_omnibox_input.h"

#import <unordered_set>

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_scheme_classifier.h"
#import "ios/web_view/internal/web_view_global_state_util.h"
#import "net/base/apple/url_conversions.h"

namespace {

// Implementation of AutocompleteSchemeClassifier for CWVOmniboxInput.
class WebViewSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  WebViewSchemeClassifier() = default;

  WebViewSchemeClassifier(const WebViewSchemeClassifier&) = delete;
  WebViewSchemeClassifier& operator=(const WebViewSchemeClassifier&) = delete;

  ~WebViewSchemeClassifier() override = default;

  // Overridden from AutocompleteSchemeClassifier:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;

 private:
  static const std::unordered_set<std::string>& supported_schemes();
};

metrics::OmniboxInputType WebViewSchemeClassifier::GetInputTypeForScheme(
    const std::string& scheme) const {
  if (supported_schemes().count(scheme) > 0) {
    return metrics::OmniboxInputType::URL;
  } else {
    return metrics::OmniboxInputType::EMPTY;
  }
}

const std::unordered_set<std::string>&
WebViewSchemeClassifier::supported_schemes() {
  static std::unordered_set<std::string> schemes;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // HTTP(S) schemes and AppStore schemes are supported.
    schemes = {url::kHttpScheme, url::kHttpsScheme, "itms",
               "itmss",          "itms-apps",       "itms-appss",
               "itms-books",     "itms-bookss"};
  });
  return schemes;
}

}  // namespace

@implementation CWVOmniboxInput {
  AutocompleteInput _autocompleteInput;
}

+ (void)initialize {
  if (self != [CWVOmniboxInput self]) {
    return;
  }

  // AutocompleteInput depends on ICU, which is initialized as part of the
  // global initialization. CWVOmniboxInput can be used independently from
  // CWVWebView, so it must be initialized here.
  ios_web_view::InitializeGlobalState();
}

- (instancetype)initWithText:(NSString*)inputText
    shouldUseHTTPSAsDefaultScheme:(BOOL)shouldUseHTTPSAsDefaultScheme {
  if ((self = [super init])) {
    std::u16string u16InputText = base::SysNSStringToUTF16(inputText);
    _autocompleteInput = AutocompleteInput(
        u16InputText, /* cursor_position = */ std::u16string::npos,
        /* desired_tld = */ "", metrics::OmniboxEventProto::OTHER,
        WebViewSchemeClassifier(), shouldUseHTTPSAsDefaultScheme);
  }
  return self;
}

- (NSString*)text {
  return base::SysUTF16ToNSString(_autocompleteInput.text());
}

- (CWVOmniboxInputType)type {
  switch (_autocompleteInput.type()) {
    case metrics::OmniboxInputType::EMPTY:
      return CWVOmniboxInputTypeEmpty;
    case metrics::OmniboxInputType::UNKNOWN:
      return CWVOmniboxInputTypeUnknown;
    case metrics::OmniboxInputType::URL:
    case metrics::OmniboxInputType::DEPRECATED_REQUESTED_URL:
      return CWVOmniboxInputTypeURL;
    case metrics::OmniboxInputType::QUERY:
    case metrics::OmniboxInputType::DEPRECATED_FORCED_QUERY:
      return CWVOmniboxInputTypeQuery;
  }
}

- (NSURL*)URL {
  return net::NSURLWithGURL(_autocompleteInput.canonicalized_url());
}

- (BOOL)addedHTTPSToTypedURL {
  // `added_default_scheme_to_typed_url()` returns true only when it has added
  // HTTPS (but not when it has added HTTP).
  return _autocompleteInput.added_default_scheme_to_typed_url();
}

@end
