// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"

class WebOmniboxEditController;

namespace ios {
class ChromeBrowserState;
}

class ChromeOmniboxClientIOS : public OmniboxClient {
 public:
  ChromeOmniboxClientIOS(WebOmniboxEditController* controller,
                         ios::ChromeBrowserState* browser_state);
  ~ChromeOmniboxClientIOS() override;

  // OmniboxClient.
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  std::unique_ptr<OmniboxNavigationObserver> CreateOmniboxNavigationObserver(
      const base::string16& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternate_nav_match) override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  bool IsDefaultSearchProviderEnabled() const override;
  const SessionID& GetSessionID() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  bool ProcessExtensionKeyword(const TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               OmniboxNavigationObserver* observer) override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnResultChanged(
      const AutocompleteResult& result,
      bool default_match_changed,
      const base::Callback<void(int result_index, const SkBitmap& bitmap)>&
          on_bitmap_fetched) override;
  void OnBookmarkLaunched() override;
  void DiscardNonCommittedNavigations() override;
  const base::string16& GetTitle() const override;
  gfx::Image GetFavicon() const override;

 private:
  WebOmniboxEditController* controller_;
  ios::ChromeBrowserState* browser_state_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxClientIOS);
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
