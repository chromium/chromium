// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/interceptable_pref_filter.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestInterceptablePrefFilter final : public InterceptablePrefFilter {
 public:
  void FilterUpdate(std::string_view path) override {}

  OnWriteCallbackPair FilterSerializeData(
      base::Value::Dict& pref_store_contents) override {
    return {};
  }

 private:
  void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      base::Value::Dict pref_store_contents,
      bool prefs_altered) override {
    std::move(post_filter_on_load_callback)
        .Run(std::move(pref_store_contents), prefs_altered);
  }

  base::WeakPtr<InterceptablePrefFilter> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<InterceptablePrefFilter> weak_ptr_factory_{this};
};

void NoOpIntercept(InterceptablePrefFilter::FinalizeFilterOnLoadCallback
                       finalize_filter_on_load,
                   base::Value::Dict prefs) {
  std::move(finalize_filter_on_load).Run(std::move(prefs), false);
}

void DeleteFilter(std::unique_ptr<TestInterceptablePrefFilter>* filter,
                  base::Value::Dict prefs,
                  bool schedule_write) {
  filter->reset();
}

TEST(InterceptablePrefFilterTest, CallbackDeletes) {
  auto filter = std::make_unique<TestInterceptablePrefFilter>();
  filter->InterceptNextFilterOnLoad(base::BindOnce(&NoOpIntercept));
  filter->FilterOnLoad(base::BindOnce(&DeleteFilter, &filter),
                       base::Value::Dict());
  EXPECT_FALSE(filter);
}

}  // namespace
