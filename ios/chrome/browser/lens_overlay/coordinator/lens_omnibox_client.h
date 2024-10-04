// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@protocol LensOmniboxClientDelegate;
@protocol LensWebProvider;
namespace feature_engagement {
class Tracker;
}

/// OmniboxClient for the omnibox in the lens overlay.
/// Interface that allows the omnibox component to interact with its embedder.
class LensOmniboxClient final : public OmniboxClient {
 public:
  LensOmniboxClient(ProfileIOS* profile,
                    feature_engagement::Tracker* tracker,
                    id<LensWebProvider> web_provider,
                    id<LensOmniboxClientDelegate> omnibox_delegate);

  LensOmniboxClient(const LensOmniboxClient&) = delete;
  LensOmniboxClient& operator=(const LensOmniboxClient&) = delete;

  ~LensOmniboxClient() override;

  void SetLensOverlaySuggestInputs(
      std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs) {
    lens_overlay_suggest_inputs_ = suggest_inputs;
  }

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
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

  bool ProcessExtensionKeyword(const std::u16string& text,
                               const TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition) override;
  void DiscardNonCommittedNavigations() override;
  const std::u16string& GetTitle() const override;
  gfx::Image GetFavicon() const override;
  void OnThumbnailRemoved() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
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
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

 private:
  raw_ptr<ProfileIOS> profile_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
  raw_ptr<feature_engagement::Tracker> engagement_tracker_;
  __weak id<LensWebProvider> web_provider_;
  __weak id<LensOmniboxClientDelegate> delegate_;
  BOOL thumbnail_removed_in_session_;
  std::optional<lens::proto::LensOverlaySuggestInputs>
      lens_overlay_suggest_inputs_;

  base::WeakPtrFactory<LensOmniboxClient> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_H_
