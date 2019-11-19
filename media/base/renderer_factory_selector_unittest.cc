// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/overlay_info.h"
#include "media/base/renderer_factory_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class RendererFactorySelectorTest : public testing::Test {
 public:
  using FactoryType = RendererFactorySelector::FactoryType;

  class FakeFactory : public RendererFactory {
   public:
    explicit FakeFactory(FactoryType type) : type_(type) {}

    std::unique_ptr<Renderer> CreateRenderer(
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
        const scoped_refptr<base::TaskRunner>& worker_task_runner,
        AudioRendererSink* audio_renderer_sink,
        VideoRendererSink* video_renderer_sink,
        const RequestOverlayInfoCB& request_overlay_info_cb,
        const gfx::ColorSpace& target_color_space) override {
      return nullptr;
    }

    FactoryType factory_type() { return type_; }

   private:
    FactoryType type_;
  };

  RendererFactorySelectorTest() = default;

  void AddBaseFactory(FactoryType type) {
    selector_.AddBaseFactory(type, std::make_unique<FakeFactory>(type));
  }

  void AddFactory(FactoryType type) {
    selector_.AddFactory(type, std::make_unique<FakeFactory>(type));
  }

  void AddConditionalFactory(FactoryType type) {
    condition_met_map_[type] = false;
    selector_.AddConditionalFactory(
        type, std::make_unique<FakeFactory>(type),
        base::Bind(&RendererFactorySelectorTest::IsConditionMet,
                   base::Unretained(this), type));
  }

  FactoryType GetCurrentlySelectedFactoryType() {
    return reinterpret_cast<FakeFactory*>(selector_.GetCurrentFactory())
        ->factory_type();
  }

  bool IsConditionMet(FactoryType type) {
    DCHECK(condition_met_map_.count(type));
    return condition_met_map_[type];
  }

 protected:
  RendererFactorySelector selector_;
  std::map<FactoryType, bool> condition_met_map_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactorySelectorTest);
};

TEST_F(RendererFactorySelectorTest, SingleFactory) {
  AddBaseFactory(FactoryType::DEFAULT);
  EXPECT_EQ(FactoryType::DEFAULT, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, MultipleFactory) {
  AddBaseFactory(FactoryType::DEFAULT);
  AddFactory(FactoryType::MOJO);

  EXPECT_EQ(FactoryType::DEFAULT, GetCurrentlySelectedFactoryType());

  selector_.SetBaseFactoryType(FactoryType::MOJO);
  EXPECT_EQ(FactoryType::MOJO, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, ConditionalFactory) {
  AddBaseFactory(FactoryType::DEFAULT);
  AddFactory(FactoryType::MOJO);
  AddConditionalFactory(FactoryType::COURIER);

  EXPECT_EQ(FactoryType::DEFAULT, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::COURIER] = true;
  EXPECT_EQ(FactoryType::COURIER, GetCurrentlySelectedFactoryType());

  selector_.SetBaseFactoryType(FactoryType::MOJO);
  EXPECT_EQ(FactoryType::COURIER, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::COURIER] = false;
  EXPECT_EQ(FactoryType::MOJO, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, MultipleConditionalFactories) {
  AddBaseFactory(FactoryType::DEFAULT);
  AddConditionalFactory(FactoryType::FLINGING);
  AddConditionalFactory(FactoryType::COURIER);

  EXPECT_EQ(FactoryType::DEFAULT, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::FLINGING] = false;
  condition_met_map_[FactoryType::COURIER] = true;
  EXPECT_EQ(FactoryType::COURIER, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::FLINGING] = true;
  condition_met_map_[FactoryType::COURIER] = false;
  EXPECT_EQ(FactoryType::FLINGING, GetCurrentlySelectedFactoryType());

  // It's up to the implementation detail to decide which one to use.
  condition_met_map_[FactoryType::FLINGING] = true;
  condition_met_map_[FactoryType::COURIER] = true;
  EXPECT_TRUE(GetCurrentlySelectedFactoryType() == FactoryType::FLINGING ||
              GetCurrentlySelectedFactoryType() == FactoryType::COURIER);
}

}  // namespace media
