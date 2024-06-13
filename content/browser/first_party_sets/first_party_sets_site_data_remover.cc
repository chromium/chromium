// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_site_data_remover.h"

#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace content {

namespace {

class ClearingTask : public BrowsingDataRemover::Observer {
 public:
  ClearingTask(BrowsingDataRemover& remover,
               std::vector<net::SchemefulSite> sites,
               base::OnceCallback<void(uint64_t)> callback)
      : remover_(remover),
        sites_(std::move(sites)),
        callback_(std::move(callback)) {
    scoped_observation_.Observe(&*remover_);
  }
  ~ClearingTask() override {
    // This FirstPartySetsSiteDataClearer class is self-owned, and the only way
    // for it to be destroyed should be the "delete this" part in
    // OnBrowsingDataRemoverDone() function, and it invokes the |callback_|. So
    // when this destructor is called, the |callback_| should be null.
    CHECK(!callback_);
  }

  void RunAndDestroySelfWhenDone() {
    std::unique_ptr<BrowsingDataFilterBuilder> cookie_filter_builder(
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kDelete));
    std::unique_ptr<BrowsingDataFilterBuilder> origin_filter_builder(
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kDelete));

    for (const auto& site : sites_) {
      // For clearing eTLD+1 scoped data.
      cookie_filter_builder->AddRegisterableDomain(site.GetURL().host());
      // For clearing origin-scoped data.
      origin_filter_builder->AddOrigin(url::Origin::Create(site.GetURL()));
    }

    task_count_++;
    remover_->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(cookie_filter_builder), this);

    uint64_t remove_mask =
        BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
        BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX |
        BrowsingDataRemover::DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS;
    task_count_++;
    remover_->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(origin_filter_builder), this);

    CHECK_GT(task_count_, 0);
  }

 private:
  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    CHECK_GT(task_count_, 0);
    failed_data_types_ |= failed_data_types;
    if (--task_count_)
      return;
    scoped_observation_.Reset();
    std::move(callback_).Run(failed_data_types_);
    delete this;
  }

  raw_ref<BrowsingDataRemover> remover_;
  std::vector<net::SchemefulSite> sites_;
  base::OnceCallback<void(uint64_t)> callback_;
  int task_count_ = 0;
  uint64_t failed_data_types_ = 0;
  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemover::Observer>
      scoped_observation_{this};
};

}  // namespace

// static
void FirstPartySetsSiteDataRemover::RemoveSiteData(
    BrowsingDataRemover& remover,
    std::vector<net::SchemefulSite> sites,
    base::OnceCallback<void(uint64_t)> callback) {
  if (sites.empty()) {
    std::move(callback).Run(0);
    return;
  }
  (new ClearingTask(remover, std::move(sites), std::move(callback)))
      ->RunAndDestroySelfWhenDone();
}

}  // namespace content
