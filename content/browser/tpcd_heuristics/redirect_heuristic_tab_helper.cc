// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/redirect_heuristic_tab_helper.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_metrics.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

WEB_CONTENTS_USER_DATA_KEY_IMPL(RedirectHeuristicTabHelper);

RedirectHeuristicTabHelper::RedirectHeuristicTabHelper(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<RedirectHeuristicTabHelper>(*web_contents),
      detector_(RedirectChainDetector::FromWebContents(web_contents)),
      dips_service_(BtmServiceImpl::Get(web_contents->GetBrowserContext())) {
  obs_.Observe(detector_);
}

RedirectHeuristicTabHelper::~RedirectHeuristicTabHelper() = default;

void RedirectHeuristicTabHelper::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  if (!render_frame_host->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kPrerendering)) {
    // Record a RedirectHeuristic UKM event if applicable. We cannot record it
    // while prerendering due to our data collection policy.
    MaybeRecordRedirectHeuristic(render_frame_host->GetPageUkmSourceId(),
                                 details);
  }
}

void RedirectHeuristicTabHelper::PrimaryPageChanged(Page& page) {
  last_commit_timestamp_ = clock_->Now();
}

void RedirectHeuristicTabHelper::OnNavigationCommitted(
    NavigationHandle* navigation_handle) {
  // Use the redirects just added to the BtmRedirectContext in order to
  // create new storage access grants when the Redirect heuristic applies.
  CreateAllRedirectHeuristicGrants(web_contents()->GetLastCommittedURL());
}

void RedirectHeuristicTabHelper::MaybeRecordRedirectHeuristic(
    const ukm::SourceId& first_party_source_id,
    const CookieAccessDetails& details) {
  if (!dips_service_) {
    // If there's no DIPS service, we can't check for past interactions.
    return;
  }

  const std::string first_party_site = GetSiteForBtm(details.first_party_url);
  const std::string third_party_site = GetSiteForBtm(details.url);
  if (first_party_site == third_party_site) {
    // The redirect heuristic does not apply for first-party cookie access.
    return;
  }

  auto third_party_site_info =
      detector_->CommittedRedirectContext().GetRedirectInfoFromChain(
          third_party_site);
  if (!third_party_site_info.has_value()) {
    // The redirect heuristic doesn't apply if the third party is not in the
    // current redirect chain.
    return;
  }
  size_t third_party_site_index = third_party_site_info->first;
  ukm::SourceId third_party_source_id =
      third_party_site_info->second->redirector_source_id;
  bool is_current_interaction =
      detector_->CommittedRedirectContext().SiteHadUserActivationOrAuthn(
          third_party_site);

  auto first_party_site_info =
      detector_->CommittedRedirectContext().GetRedirectInfoFromChain(
          first_party_site);
  size_t first_party_site_index;
  if (!first_party_site_info.has_value() ||
      first_party_site_info->first < third_party_site_index) {
    // If the first-party site does not appear in the list of committed
    // redirects after the third-party site, that must mean it's in an ongoing
    // navigation.
    first_party_site_index = detector_->CommittedRedirectContext().size();
  } else {
    first_party_site_index = first_party_site_info->first;
  }
  const size_t sites_passed_count =
      first_party_site_index - third_party_site_index;

  CHECK(dips_service_);
  CHECK(!dips_service_->storage()->is_null());

  auto callback = base::BindOnce(
      [](base::WeakPtr<RedirectHeuristicTabHelper> service,
         const ukm::SourceId& first_party_source_id,
         const ukm::SourceId& third_party_source_id,
         const CookieAccessDetails& details, const size_t sites_passed_count,
         bool is_current_interaction,
         std::pair<std::optional<base::Time>, BtmInteractionType> range) {
        if (!service) {
          return;
        }
        return service->RecordRedirectHeuristic(
            first_party_source_id, third_party_source_id, details,
            sites_passed_count, is_current_interaction, range.second,
            range.first);
      },
      weak_factory_.GetWeakPtr(), first_party_source_id, third_party_source_id,
      details, sites_passed_count, is_current_interaction);

  dips_service_->storage()
      ->AsyncCall(&BtmStorage::LastInteractionTimeAndType)
      .WithArgs(details.url)
      .Then(std::move(callback));
}

void RedirectHeuristicTabHelper::RecordRedirectHeuristic(
    const ukm::SourceId& first_party_source_id,
    const ukm::SourceId& third_party_source_id,
    const CookieAccessDetails& details,
    const size_t sites_passed_count,
    bool is_current_interaction,
    BtmInteractionType interaction_type,
    std::optional<base::Time> last_user_interaction_time) {
  // This function can only be reached if the redirect heuristic is satisfied
  // for the previous recorded redirect.
  DCHECK(last_commit_timestamp_.has_value());
  int32_t milliseconds_since_redirect = Bucketize3PCDHeuristicSample(
      (clock_->Now() - last_commit_timestamp_.value()).InMilliseconds(),
      base::Minutes(15).InMilliseconds());

  int32_t hours_since_last_interaction = -1;
  if (last_user_interaction_time.has_value()) {
    hours_since_last_interaction = Bucketize3PCDHeuristicSample(
        (clock_->Now() - last_user_interaction_time.value()).InHours(),
        base::Days(60).InHours());
  }

  OptionalBool has_same_site_iframe =
      ToOptionalBool(HasSameSiteIframe(web_contents(), details.url));
  OptionalBool is_ad_tagged_cookie = IsAdTaggedCookieForHeuristics(details);

  const bool first_party_precedes_third_party =
      AllSitesFollowingFirstParty(web_contents(), details.first_party_url)
          .contains(GetSiteForBtm(details.url));

  int32_t access_id = base::RandUint64();

  ukm::builders::RedirectHeuristic_CookieAccess2(first_party_source_id)
      .SetAccessId(access_id)
      .SetAccessAllowed(!details.blocked_by_policy)
      .SetIsAdTagged(static_cast<int64_t>(is_ad_tagged_cookie))
      .SetHoursSinceLastInteraction(hours_since_last_interaction)
      .SetMillisecondsSinceRedirect(milliseconds_since_redirect)
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_same_site_iframe))
      .SetSitesPassedCount(sites_passed_count)
      .SetDoesFirstPartyPrecedeThirdParty(first_party_precedes_third_party)
      .SetIsCurrentInteraction(is_current_interaction)
      .SetInteractionType(static_cast<int64_t>(interaction_type))
      .Record(ukm::UkmRecorder::Get());

  ukm::builders::RedirectHeuristic_CookieAccessThirdParty2(
      third_party_source_id)
      .SetAccessId(access_id)
      .Record(ukm::UkmRecorder::Get());
}

void RedirectHeuristicTabHelper::CreateAllRedirectHeuristicGrants(
    const GURL& first_party_url) {

  base::TimeDelta grant_duration =
      content_settings::features::kTpcdWriteRedirectHeuristicGrants.Get();
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kTpcdHeuristicsGrants) ||
      !grant_duration.is_positive()) {
    return;
  }

  std::optional<std::set<std::string>> sites_with_aba_flow = std::nullopt;
  if (content_settings::features::kTpcdRedirectHeuristicRequireABAFlow.Get()) {
    sites_with_aba_flow =
        AllSitesFollowingFirstParty(web_contents(), first_party_url);
  }

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction = GetRedirectHeuristicURLs(
          detector_->CommittedRedirectContext(), first_party_url,
          sites_with_aba_flow,
          content_settings::features::
              kTpcdRedirectHeuristicRequireCurrentInteraction.Get());

  for (const auto& kv : sites_to_url_and_current_interaction) {
    auto [url, is_current_interaction] = kv.second;
    // If there was a current interaction, there is no need to call BtmStorage
    // to check the db for a past interaction.
    if (is_current_interaction) {
      CreateRedirectHeuristicGrant(url, first_party_url, grant_duration,
                                   /*has_interaction=*/true);
    } else {
      // If `kTpcdRedirectHeuristicRequireCurrentInteraction` is true, then
      // `GetRedirectHeuristicURLs` should not return any entries without a
      // current interaction.
      DCHECK(!content_settings::features::
                  kTpcdRedirectHeuristicRequireCurrentInteraction.Get());
      base::OnceCallback<void(std::optional<base::Time>)> create_grant =
          base::BindOnce(
              [](std::optional<base::Time> last_user_interaction_time) {
                return last_user_interaction_time.has_value();
              })
              .Then(base::BindOnce(
                  &RedirectHeuristicTabHelper::CreateRedirectHeuristicGrant,
                  weak_factory_.GetWeakPtr(), url, first_party_url,
                  grant_duration));

      CHECK(dips_service_);
      CHECK(!dips_service_->storage()->is_null());
      dips_service_->storage()
          ->AsyncCall(&BtmStorage::LastUserActivationOrAuthnAssertionTime)
          .WithArgs(url)
          .Then(std::move(create_grant));
    }
  }
}

void RedirectHeuristicTabHelper::CreateRedirectHeuristicGrant(
    const GURL& url,
    const GURL& first_party_url,
    base::TimeDelta grant_duration,
    bool has_interaction) {
  if (has_interaction) {
    // TODO(crbug.com/40282235): Make these grants lossy to avoid spamming
    // profile prefs with blocking requests.
    // TODO(crbug.com/40282235): Add bounds to these grants to avoid overflow.
    // TODO(crbug.com/40282235): Consider applying these grants only to rSA
    // calls.
    GetContentClient()->browser()->GrantCookieAccessDueToHeuristic(
        web_contents()->GetBrowserContext(),
        net::SchemefulSite(first_party_url), net::SchemefulSite(url),
        grant_duration, /*ignore_schemes=*/false);
  }
}

/* static */
std::set<std::string> RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
    WebContents* web_contents,
    const GURL& first_party_url) {
  std::set<std::string> sites;

  NavigationController& nav_controller = web_contents->GetController();
  const std::string first_party_site = GetSiteForBtm(first_party_url);

  int min_index = std::max(0, nav_controller.GetCurrentEntryIndex() -
                                  kAllSitesFollowingFirstPartyLookbackLength);
  std::optional<std::string> prev_site = std::nullopt;
  for (int ind = nav_controller.GetCurrentEntryIndex(); ind >= min_index;
       ind--) {
    std::string cur_site =
        GetSiteForBtm(nav_controller.GetEntryAtIndex(ind)->GetURL());

    if (cur_site == first_party_site) {
      if (prev_site.has_value() && *prev_site != first_party_site) {
        sites.insert(*prev_site);
      }
    }

    prev_site = cur_site;
  }

  return sites;
}

void RedirectHeuristicTabHelper::WebContentsDestroyed() {
  detector_ = nullptr;  // was observing the same WebContents.
  obs_.Reset();
}

}  // namespace content
