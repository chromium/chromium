// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_CHROME_OMNIBOX_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_CHROME_OMNIBOX_CLIENT_IOS_H_

#import <memory>
#import <unordered_map>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"

class Browser;
class ProfileIOS;

class WebLocationBar;
namespace feature_engagement {
class Tracker;
}

class ChromeOmniboxClientIOS final : public OmniboxClient {
 public:
  ChromeOmniboxClientIOS(WebLocationBar* location_bar,
                         Browser* browser,
                         feature_engagement::Tracker* tracker);

  ChromeOmniboxClientIOS(const ChromeOmniboxClientIOS&) = delete;
  ChromeOmniboxClientIOS& operator=(const ChromeOmniboxClientIOS&) = delete;

  ~ChromeOmniboxClientIOS() override;

  // OmniboxClient.
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  bool IsDefaultSearchProviderEnabled() const override;
  SessionID GetSessionID() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetExtensionIcon(const TemplateURL* template_url) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  void ProcessExtensionMatch(const std::u16string& text,
                             const TemplateURL* template_url,
                             const AutocompleteMatch& match,
                             WindowOpenDisposition disposition) override;
  void OnUserPastedInOmniboxResultingInValidURL() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       bool should_prerender,
                       const BitmapFetchedCallback& on_bitmap_fetched) override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void DiscardNonCommittedNavigations() override;
  const std::u16string& GetTitle() const override;
  gfx::Image GetFavicon() const override;
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

 private:
  raw_ptr<WebLocationBar> location_bar_;
  raw_ptr<Browser> browser_;
  raw_ptr<ProfileIOS> profile_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
  raw_ptr<feature_engagement::Tracker> engagement_tracker_;

  base::WeakPtrFactory<ChromeOmniboxClientIOS> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_CHROME_OMNIBOX_CLIENT_IOS_H_
