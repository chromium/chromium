// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_

#include <memory>

#include "components/omnibox/browser/omnibox_client.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"

class ChromeBrowserState;
class WebLocationBar;
namespace feature_engagement {
class Tracker;
}

class ChromeOmniboxClientIOS : public OmniboxClient {
 public:
  ChromeOmniboxClientIOS(WebLocationBar* location_bar,
                         ChromeBrowserState* browser_state,
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
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  bool ProcessExtensionKeyword(const std::u16string& text,
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
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  LocationBarModel* GetLocationBarModel() override;

 private:
  WebLocationBar* location_bar_;
  ChromeBrowserState* browser_state_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
  feature_engagement::Tracker* engagement_tracker_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
