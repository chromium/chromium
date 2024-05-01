// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_worklet_util.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"

namespace auction_worklet {

class AuctionWorkletUtilTest : public testing::Test {
 public:
  AuctionWorkletUtilTest() {
    v8_helper_ = AuctionV8Helper::Create(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    // Here since we're using the same thread for everything, we need to spin
    // the event loop to let AuctionV8Helper finish initializing "off-thread";
    // normally PostTask semantics will ensure that anything that uses it on its
    // thread would happen after such initialization.
    base::RunLoop().RunUntilIdle();
    v8_scope_ =
        std::make_unique<AuctionV8Helper::FullIsolateScope>(v8_helper_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

TEST_F(AuctionWorkletUtilTest, CanSetAdSize) {
  EXPECT_FALSE(CanSetAdSize(std::nullopt));
  EXPECT_FALSE(
      CanSetAdSize(blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                 50, blink::AdSize::LengthUnit::kInvalid)));
  EXPECT_TRUE(
      CanSetAdSize(blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                 50, blink::AdSize::LengthUnit::kPixels)));
}

TEST_F(AuctionWorkletUtilTest, MaybeSetSizeMemberSuccess) {
  v8::Context::Scope ctx(v8_helper_->scratch_context());
  v8::Isolate* isolate = v8_helper_->isolate();

  gin::Dictionary top_level_dict = gin::Dictionary::CreateEmpty(isolate);
  blink::AdSize ad_size =
      blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                    blink::AdSize::LengthUnit::kPixels);

  // This should result in a dictionary of:
  // {"renderSize": {"width": "100sw", "height": "50px"}}
  EXPECT_TRUE(
      MaybeSetSizeMember(isolate, top_level_dict, "renderSize", ad_size));

  gin::Dictionary size_dict = gin::Dictionary::CreateEmpty(isolate);
  EXPECT_TRUE(top_level_dict.Get("renderSize", &size_dict));

  std::string width;
  EXPECT_TRUE(size_dict.Get("width", &width));
  EXPECT_EQ(width, "100sw");

  std::string height;
  EXPECT_TRUE(size_dict.Get("height", &height));
  EXPECT_EQ(height, "50px");
}

}  // namespace auction_worklet
