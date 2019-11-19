// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/interceptable_pref_filter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestInterceptablePrefFilter : public InterceptablePrefFilter {
 public:
  void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) override {
    std::move(post_filter_on_load_callback)
        .Run(std::move(pref_store_contents), prefs_altered);
  }

  void FilterUpdate(const std::string& path) override {}

  OnWriteCallbackPair FilterSerializeData(
      base::DictionaryValue* pref_store_contents) override {
    return {};
  }
};

void NoOpIntercept(InterceptablePrefFilter::FinalizeFilterOnLoadCallback
                       finalize_filter_on_load,
                   std::unique_ptr<base::DictionaryValue> prefs) {
  std::move(finalize_filter_on_load).Run(std::move(prefs), false);
}

void DeleteFilter(std::unique_ptr<TestInterceptablePrefFilter>* filter,
                  std::unique_ptr<base::DictionaryValue> prefs,
                  bool schedule_write) {
  filter->reset();
}

TEST(InterceptablePrefFilterTest, CallbackDeletes) {
  auto filter = std::make_unique<TestInterceptablePrefFilter>();
  filter->InterceptNextFilterOnLoad(base::BindOnce(&NoOpIntercept));
  filter->FilterOnLoad(base::BindOnce(&DeleteFilter, &filter),
                       std::make_unique<base::DictionaryValue>());
  EXPECT_FALSE(filter);
}

}  // namespace
