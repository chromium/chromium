---
name: privacy/browsing-data-remover-deletion-test-coverage
description: >-
  Checks that new BrowsingDataRemover integrations come with tests that
  exercise the remover itself across full and partial time ranges and
  origin/domain filters, not only the target service's deletion method.
rubric: testing
severity: should_fix
---

**Rule.** Flag CLs that add a new data deletion integration to
`BrowsingDataRemoverImpl` or `ChromeBrowsingDataRemoverDelegate` without tests
that exercise the `BrowsingDataRemover` integration itself. Unit tests on the
target service's deletion method alone are NOT sufficient. The following
coverage is required:

1. **Remover integration test:** Either a browser test (in
   `browsing_data_remover_browsertest.cc` or
   `browsing_data_remover_impl_browsertest.cc`) that populates data, runs
   `BrowsingDataRemover`, and verifies removal; or a delegate/remover unit test
   (in `chrome_browsing_data_remover_delegate_unittest.cc` or
   `browsing_data_remover_impl_unittest.cc`) verifying the call's time range,
   filter, and completion callback alongside service unit tests.
2. **Time-range coverage:** Both **full time-range ("All time") deletion** and
   **partial time-range deletion** MUST be tested. For timestamped stores, the
   partial time-range test MUST populate at least two entries at different
   timestamps and verify that `[delete_begin, delete_end)` removes only the
   in-range entry. For stores without timestamps, the partial time-range test
   MUST document the chosen behavior (see
   `privacy/browsing-data-remover-time-range-filtering`): either that the whole
   store is cleared regardless of the selected range, or that nothing is cleared
   unless "All time" is selected.
3. **Origin/domain filter coverage:** For datatypes in `FILTERABLE_DATA_TYPES`
   (Cookies/Site Data and Cache), per-origin or per-domain filtered deletion
   (using `BrowsingDataFilterBuilder`) MUST be tested with data for at least two
   origins/domains, verifying that only the matching origin/domain is deleted.

**Anchor.** Anchor the finding on the newly added deletion call in
`BrowsingDataRemoverImpl` or `ChromeBrowsingDataRemoverDelegate`, or on the
incomplete test in `*_unittest.cc` / `*_browsertest.cc`.

**Why.** Testing only the service's `ClearData()` method or only "All time"
deletion misses broken `DataType` masks, time windows, origin filters, and async
completion callbacks in `BrowsingDataRemover`.

## Examples

Bad:

```cpp
// `my_service->ClearData(...)` was added to
// ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData, but only
// `MyService::ClearData` is tested directly in my_service_unittest.cc, with an
// All-Time range and no filter, and ChromeBrowsingDataRemoverDelegate is not
// tested at all.
TEST_F(MyServiceTest, ClearData) {
  service_->AddEntry(GURL("https://example.com"), base::Time::Now());
  service_->ClearData(base::Time(), base::Time::Max(), base::NullCallback());
  EXPECT_TRUE(service_->GetEntries().empty());
}
```

Good:

```cpp
// chrome_browsing_data_remover_delegate_unittest.cc: The delegate wiring is
// tested across the full time range, a partial time range (with two
// timestamps), and origin filtering.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveMyServiceData_AllTime) {
  auto* service = MyServiceFactory::GetForProfile(GetProfile());
  service->AddEntry(GURL("https://example.com"), base::Time::Now());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, false);

  EXPECT_TRUE(service->GetEntries().empty());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveMyServiceData_TimeRangeAndFilter) {
  auto* service = MyServiceFactory::GetForProfile(GetProfile());
  const base::Time t1 = base::Time::Now() - base::Hours(2);
  const base::Time t2 = base::Time::Now() - base::Minutes(30);
  service->AddEntry(GURL("https://a.example.com"), t1);
  service->AddEntry(GURL("https://a.example.com"), t2);
  service->AddEntry(GURL("https://b.example.com"), t2);

  // Partial time-range + origin-filtered deletion for a.example.com in the last hour.
  std::unique_ptr<BrowsingDataFilterBuilder> filter =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddOrigin(url::Origin::Create(GURL("https://a.example.com")));
  BlockUntilOriginDataRemoved(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, std::move(filter));

  // Entry at t1 (outside time range) and entry for b.example.com (outside origin
  // filter) are preserved; entry for a.example.com at t2 is deleted.
  EXPECT_THAT(service->GetEntries(),
              UnorderedElementsAre(Pair(GURL("https://a.example.com"), t1),
                                   Pair(GURL("https://b.example.com"), t2)));
}
```

Good:

```cpp
// For a store without timestamps, a partial time-range test documents that the
// whole store is cleared regardless of the selected range.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveTimelessServiceData_PartialRangeClearsEverything) {
  auto* service = TimelessServiceFactory::GetForProfile(GetProfile());
  service->AddEntry(GURL("https://example.com"));

  BlockUntilBrowsingDataRemoved(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA, false);

  EXPECT_TRUE(service->GetEntries().empty());
}
```
