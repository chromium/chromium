// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"

#include <vector>

#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

// Mock `PrefetchContainer` to test `CollectMatchCandidatesGeneric()`.
class MockContainer {
 public:
  struct Args {
    blink::DocumentToken document_token;
    GURL url;
    PrefetchContainer::ServableState servable_state;
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint;
    std::optional<net::HttpNoVarySearchData> no_vary_search_data;
  };

  explicit MockContainer(MockContainer::Args args)
      : key_(PrefetchContainer::Key(args.document_token, args.url)),
        servable_state_(args.servable_state),
        no_vary_search_hint_(args.no_vary_search_hint),
        no_vary_search_data_(args.no_vary_search_data),
        prefetch_status_(PrefetchStatus::kPrefetchSuccessful) {}
  ~MockContainer() = default;

  const GURL& GetURL() const { return key_.url(); }

  PrefetchContainer::ServableState GetServableState(
      base::TimeDelta cacheable_duration) const {
    return servable_state_;
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
    // Use `servable_state_` instead.
    bool simulate_get_non_redirect_head_is_null =
        (servable_state_ != PrefetchContainer::ServableState::kServable);
    return simulate_get_non_redirect_head_is_null && no_vary_search_hint &&
           no_vary_search_hint->AreEquivalent(url, GetURL());
  }

  // We don't test on this property.
  bool HasPrefetchBeenConsideredToServe() const { return false; }

  // We don't test on this property.
  bool IsDecoy() const { return false; }

  void SetServingPageMetrics(base::WeakPtr<PrefetchServingPageMetricsContainer>
                                 serving_page_metrics_container) {}
  void UpdateServingPageMetrics() {}

  const PrefetchContainer::Key& key() const { return key_; }
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint() const {
    return no_vary_search_hint_;
  }
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchData() const {
    return no_vary_search_data_;
  }

 private:
  PrefetchContainer::Key key_;
  PrefetchContainer::ServableState servable_state_;
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

  std::vector<PrefetchContainer::Key> KeysOfCollectMatchCandidatesGeneric(
      const PrefetchContainer::Key& navigated_key) {
    std::vector<PrefetchContainer::Key> candidate_keys;
    // We must bind the following value instead of using `std::get()` in `for`
    // due to a lifetime issue before C++23:
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2718r0.html
    auto [candidates, _] = CollectMatchCandidatesGeneric(
        owned_prefetches_, navigated_key,
        /*serving_page_metrics_container=*/nullptr);
    for (const auto* container : candidates) {
      candidate_keys.push_back(container->key());
    }
    return candidate_keys;
  }

  void Assert(const base::Location& location,
              const PrefetchContainer::Key& navigated_key,
              const std::vector<PrefetchContainer::Key>& candidate_keys) {
    SCOPED_TRACE(::testing::Message()
                 << "from \033[31m" << location.ToString() << "\033[39m");

    EXPECT_THAT(KeysOfCollectMatchCandidatesGeneric(navigated_key),
                testing::UnorderedElementsAreArray(candidate_keys));
  }

 private:
  std::map<PrefetchContainer::Key, std::unique_ptr<MockContainer>>
      owned_prefetches_;
};

TEST(CollectMatchCandidates, DistinguishesDocumentToken) {
  using Key = PrefetchContainer::Key;

  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token1;
  blink::DocumentToken document_token2;

  helper.Add({
      .document_token = document_token1,
      .url = GURL("https://a.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
  });
  helper.Add({
      .document_token = document_token2,
      .url = GURL("https://a.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
  });

  helper.Assert(FROM_HERE, Key(document_token1, GURL("https://a.example.com/")),
                {Key(document_token1, GURL("https://a.example.com/"))});

  helper.Assert(FROM_HERE, Key(std::nullopt, GURL("https://a.example.com/")),
                {});
}

TEST(CollectMatchCandidates, DistingushesUrl) {
  using Key = PrefetchContainer::Key;

  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://b.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
  });

  helper.Assert(FROM_HERE, Key(document_token, GURL("https://a.example.com/")),
                {Key(document_token, GURL("https://a.example.com/"))});

  helper.Assert(FROM_HERE, Key(document_token, GURL("https://c.example.com/")),
                {});
}

TEST(CollectMatchCandidates, RejectsNotServable) {
  using Key = PrefetchContainer::Key;

  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://servable.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://not-servable.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kNotServable,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://should-block-until-head-received.example.com/"),
      .servable_state =
          PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived,
  });

  helper.Assert(FROM_HERE,
                Key(document_token, GURL("https://servable.example.com/")),
                {Key(document_token, GURL("https://servable.example.com/"))});

  helper.Assert(FROM_HERE,
                Key(document_token, GURL("https://not-servable.example.com/")),
                {});

  helper.Assert(
      FROM_HERE,
      Key(document_token,
          GURL("https://should-block-until-head-received.example.com/")),
      {Key(document_token,
           GURL("https://should-block-until-head-received.example.com/"))});
}

TEST(CollectMatchCandidates, ChecksNoVarySearchHintAndHeader) {
  using Key = PrefetchContainer::Key;

  CollectMatchCandidatesTestHelper helper;
  blink::DocumentToken document_token;

  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/"),
      .servable_state = PrefetchContainer::ServableState::kServable,
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?distinguish=true"),
      .servable_state = PrefetchContainer::ServableState::kServable,
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=onlyHeader"),
      .servable_state = PrefetchContainer::ServableState::kServable,
      .no_vary_search_hint = std::nullopt,
      .no_vary_search_data =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=onlyHint"),
      .servable_state =
          PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived,
      .no_vary_search_hint =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
      .no_vary_search_data = std::nullopt,
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?ignore=bothHintAndHeader"),
      .servable_state =
          PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived,
      .no_vary_search_hint =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
      .no_vary_search_data =
          net::HttpNoVarySearchData::CreateFromNoVaryParams({"ignore"}, true),
  });
  helper.Add({
      .document_token = document_token,
      .url = GURL("https://a.example.com/?distinguish=hintButContradictHeader"),
      .servable_state = PrefetchContainer::ServableState::kServable,
      .no_vary_search_hint = net::HttpNoVarySearchData::CreateFromNoVaryParams(
          {"distinguish"}, true),
      .no_vary_search_data = std::nullopt,
  });

  helper.Assert(
      FROM_HERE, Key(document_token, GURL("https://a.example.com/")),
      {
          Key(document_token, GURL("https://a.example.com/")),
          Key(document_token, GURL("https://a.example.com/?ignore=onlyHeader")),
          Key(document_token, GURL("https://a.example.com/?ignore=onlyHint")),
          Key(document_token,
              GURL("https://a.example.com/?ignore=bothHintAndHeader")),
      });
}

}  // namespace
}  // namespace content
