// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class MockPrefetchRequest {
 public:
  MockPrefetchRequest()
      : preload_pipeline_info_(
            base::WrapRefCounted(static_cast<PreloadPipelineInfoImpl*>(
                PreloadPipelineInfo::Create(
                    /*planned_max_preloading_type=*/PreloadingType::kPrerender)
                    .get()))) {}
  ~MockPrefetchRequest() = default;

  const PreloadPipelineInfoImpl& preload_pipeline_info() {
    return *preload_pipeline_info_;
  }

 private:
  scoped_refptr<PreloadPipelineInfoImpl> preload_pipeline_info_;
};

// Mock `PrefetchContainer` to test `CollectMatchCandidatesGeneric()`.
class MockContainer {
 public:
  struct Args {
    blink::DocumentToken document_token;
    GURL url;
    PrefetchMatchResolverAction match_resolver_action;
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint;
    std::optional<net::HttpNoVarySearchData> no_vary_search_data;
  };

  explicit MockContainer(MockContainer::Args args)
      : key_(PrefetchKey(args.document_token, args.url)),
        match_resolver_action_(std::move(args.match_resolver_action)),
        no_vary_search_hint_(args.no_vary_search_hint),
        no_vary_search_data_(args.no_vary_search_data),
        prefetch_status_(PrefetchStatus::kPrefetchSuccessful) {}
  ~MockContainer() = default;

  const GURL& GetURL() const { return key_.url(); }

  const PrefetchMatchResolverAction& GetMatchResolverAction() const {
    return match_resolver_action_;
  }

  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const {
    DCHECK(prefetch_status_);
    return prefetch_status_.value();
  }

  bool IsNoVarySearchHeaderMatch(const GURL& url) const {
    const std::optional<net::HttpNoVarySearchData>& no_vary_search_data =
        GetNoVarySearchData();
    return no_vary_search_data &&
           no_vary_search_data->AreEquivalent(url, GetURL());
  }

  bool ShouldWaitForNoVarySearchHeader(const GURL& url) const {
    const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint =
        GetNoVarySearchHint();
    // It's not trivial to implement `PrefetchContainer::GetNonRedirectHead()`.
    // Here, we use `match_resolver_action_` instead.
    bool simulate_get_non_redirect_head_is_null;
    switch (match_resolver_action_.prefetch_container_load_state()) {
      case PrefetchContainer::LoadState::kNotStarted:
      case PrefetchContainer::LoadState::kEligible:
      case PrefetchContainer::LoadState::kStarted:
      case PrefetchContainer::LoadState::kFailedIneligible:
      case PrefetchContainer::LoadState::kFailedDeterminedHead:
        simulate_get_non_redirect_head_is_null = true;
        break;
      case PrefetchContainer::LoadState::kDeterminedHead:
      case PrefetchContainer::LoadState::kCompleted:
      case PrefetchContainer::LoadState::kFailed:
        simulate_get_non_redirect_head_is_null = false;
        break;
      // We don't use below cases in the tests.
      case PrefetchContainer::LoadState::kFailedHeldback:
        NOTREACHED();
    }
    return simulate_get_non_redirect_head_is_null && no_vary_search_hint &&
           no_vary_search_hint->AreEquivalent(url, GetURL());
  }

  // We don't test on this property.
  bool IsDecoy() const { return false; }

  void SetServingPageMetrics(base::WeakPtr<PrefetchServingPageMetricsContainer>
                                 serving_page_metrics_container) {}
  void UpdateServingPageMetrics() {}

  const PrefetchKey& key() const { return key_; }
  MockPrefetchRequest request() const { return MockPrefetchRequest(); }
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint() const {
    return no_vary_search_hint_;
  }
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchData() const {
    return no_vary_search_data_;
  }

 private:
  PrefetchKey key_;
  PrefetchMatchResolverAction match_resolver_action_;
  std::optional<net::HttpNoVarySearchData> no_vary_search_hint_;
  std::optional<net::HttpNoVarySearchData> no_vary_search_data_;
  std::optional<PrefetchStatus> prefetch_status_;
};

std::ostream& operator<<(std::ostream& ostream, const MockContainer&) {
  return ostream;
}

class CollectMatchCandidatesTestHelper {
 public:
  CollectMatchCandidatesTestHelper() = default;
  ~CollectMatchCandidatesTestHelper() = default;

  void Add(MockContainer::Args args) {
    auto container = std::make_unique<MockContainer>(std::move(args));
    owned_prefetches_[container->key()] = std::move(container);
  }

  std::vector<PrefetchKey> KeysOfCollectMatchCandidatesGeneric(
      const PrefetchKey& navigated_key,
      bool is_nav_prerender) {
    std::vector<PrefetchKey> candidate_keys;
    // We must bind the following value instead of using `std::get()` in `for`
    // due to a lifetime issue before C++23:
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2718r0.html
    auto [candidates, _] = CollectMatchCandidatesGeneric(
        owned_prefetches_, navigated_key, is_nav_prerender,
        /*serving_page_metrics_container=*/nullptr,
        /*key_ahead_of_prerender=*/nullptr,
        /*collect_result_ahead_of_prerender=*/nullptr);
    for (const auto* container : candidates) {
      candidate_keys.push_back(container->key());
    }
    return candidate_keys;
  }

  void Assert(const base::Location& location,
              const PrefetchKey& navigated_key,
              bool is_nav_prerender,
              const std::vector<PrefetchKey>& candidate_keys) {
    SCOPED_TRACE(::testing::Message()
                 << "from \033[31m" << location.ToString() << "\033[39m");

    EXPECT_THAT(
        KeysOfCollectMatchCandidatesGeneric(navigated_key, is_nav_prerender),
        testing::UnorderedElementsAreArray(candidate_keys));
  }

 private:
  std::map<PrefetchKey, std::unique_ptr<MockContainer>> owned_prefetches_;
};

TEST(CollectMatchCandidates, DistinguishesDocumentToken) {
  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token1;
  blink::DocumentToken document_token2;

  helper.Add({
      .document_token = document_token1,
      .url = GURL("https://a.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
  });
  helper.Add({
      .document_token = document_token2,
      .url = GURL("https://a.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
  });

  helper.Assert(FROM_HERE,
                PrefetchKey(document_token1, GURL("https://a.example.com/")),
                /*is_nav_prerender=*/false,
                {PrefetchKey(document_token1, GURL("https://a.example.com/"))});

  helper.Assert(FROM_HERE,
                PrefetchKey(std::nullopt, GURL("https://a.example.com/")),
                /*is_nav_prerender=*/false, {});
}

TEST(CollectMatchCandidates, DistingushesUrl) {
  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://b.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
  });

  helper.Assert(FROM_HERE,
                PrefetchKey(document_token, GURL("https://a.example.com/")),
                /*is_nav_prerender=*/false,
                {PrefetchKey(document_token, GURL("https://a.example.com/"))});

  helper.Assert(FROM_HERE,
                PrefetchKey(document_token, GURL("https://c.example.com/")),
                /*is_nav_prerender=*/false, {});
}

TEST(CollectMatchCandidates, RejectsNotServable) {
  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://servable.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://not-servable.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kDrop,
          PrefetchContainer::LoadState::kFailed, /*is_expired=*/std::nullopt),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://should-block-until-head-received.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait,
          PrefetchContainer::LoadState::kStarted, /*is_expired=*/std::nullopt),
  });

  helper.Assert(
      FROM_HERE,
      PrefetchKey(document_token, GURL("https://servable.example.com/")),
      /*is_nav_prerender=*/false,
      {PrefetchKey(document_token, GURL("https://servable.example.com/"))});

  helper.Assert(
      FROM_HERE,
      PrefetchKey(document_token, GURL("https://not-servable.example.com/")),
      /*is_nav_prerender=*/false, {});

  helper.Assert(
      FROM_HERE,
      PrefetchKey(
          document_token,
          GURL("https://should-block-until-head-received.example.com/")),
      /*is_nav_prerender=*/false,
      {PrefetchKey(
          document_token,
          GURL("https://should-block-until-head-received.example.com/"))});
}

TEST(CollectMatchCandidates,
     IncludesShouldBlockUntilEligibilityGotIfIsLikelyAheadOfPrerender) {
  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://prerender.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait,
          PrefetchContainer::LoadState::kNotStarted,
          /*is_expired=*/std::nullopt),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://not-prerender.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait,
          PrefetchContainer::LoadState::kNotStarted,
          /*is_expired=*/std::nullopt),
  });

  helper.Assert(
      FROM_HERE,
      PrefetchKey(document_token, GURL("https://prerender.example.com/")),
      /*is_nav_prerender=*/true,
      {PrefetchKey(document_token, GURL("https://prerender.example.com/"))});

  helper.Assert(
      FROM_HERE,
      PrefetchKey(document_token, GURL("https://not-prerender.example.com/")),
      /*is_nav_prerender=*/false, {});
}

TEST(CollectMatchCandidates, ChecksNoVarySearchHintAndHeader) {
  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?distinguish=true"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=onlyHeader"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=onlyHint"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait,
          PrefetchContainer::LoadState::kStarted, /*is_expired=*/std::nullopt),
      .no_vary_search_hint =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=bothHintAndHeader"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait,
          PrefetchContainer::LoadState::kStarted, /*is_expired=*/std::nullopt),
      .no_vary_search_hint =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
      .no_vary_search_data =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?distinguish=hintButContradictHeader"),
      .match_resolver_action = PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe,
          PrefetchContainer::LoadState::kCompleted, /*is_expired=*/false),
      .no_vary_search_hint = net::HttpNoVarySearchData::CreateFromNoVaryParams(
          {"distinguish"}, true),
      .no_vary_search_data = std::nullopt,
  });

  helper.Assert(
      FROM_HERE, PrefetchKey(document_token, GURL("https://a.example.com/")),
      /*is_nav_prerender=*/false,
      {
          PrefetchKey(document_token, GURL("https://a.example.com/")),
          PrefetchKey(document_token,
                      GURL("https://a.example.com/?ignore=onlyHeader")),
          PrefetchKey(document_token,
                      GURL("https://a.example.com/?ignore=onlyHint")),
          PrefetchKey(document_token,
                      GURL("https://a.example.com/?ignore=bothHintAndHeader")),
      });
}

}  // namespace
}  // namespace content
