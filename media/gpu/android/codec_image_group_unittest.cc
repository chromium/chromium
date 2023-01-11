// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image_group.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/gpu/android/codec_surface_bundle.h"
#include "media/gpu/android/mock_codec_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
// Size used to create MockCodecImage.
constexpr gfx::Size kMockImageSize(100, 100);

// Subclass of CodecImageGroup which will notify us when it's destroyed.
class CodecImageGroupWithDestructionHook : public CodecImageGroup {
 public:
  CodecImageGroupWithDestructionHook(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<CodecSurfaceBundle> surface_bundle)
      : CodecImageGroup(std::move(task_runner),
                        std::move(surface_bundle),
                        features::NeedThreadSafeAndroidMedia()
                            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
                            : nullptr) {}

  void SetDestructionCallback(base::OnceClosure cb) {
    destruction_cb_ = std::move(cb);
  }

 private:
  ~CodecImageGroupWithDestructionHook() override {
    if (destruction_cb_)
      std::move(destruction_cb_).Run();
  }

  base::OnceClosure destruction_cb_;
};

}  // namespace

class CodecImageGroupTest : public testing::Test {
 public:
  CodecImageGroupTest() = default;

  void SetUp() override {
    gpu_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  }

  void TearDown() override {}

  struct Record {
    scoped_refptr<CodecSurfaceBundle> surface_bundle;
    scoped_refptr<CodecImageGroupWithDestructionHook> image_group;

    MockAndroidOverlay* overlay() const {
      return static_cast<MockAndroidOverlay*>(surface_bundle->overlay());
    }
  };

  // Create an image group for a surface bundle with an overlay.
  Record CreateImageGroup() {
    std::unique_ptr<MockAndroidOverlay> overlay =
        std::make_unique<MockAndroidOverlay>();
    EXPECT_CALL(*overlay.get(), MockAddSurfaceDestroyedCallback());
    Record rec;
    rec.surface_bundle =
        base::MakeRefCounted<CodecSurfaceBundle>(std::move(overlay));
    rec.image_group = base::MakeRefCounted<CodecImageGroupWithDestructionHook>(
        gpu_task_runner_, rec.surface_bundle);

    return rec;
  }

  // Handy method to check that CodecImage destruction is relayed properly.
  MOCK_METHOD1(OnCodecImageDestroyed, void(CodecImage*));

  base::test::TaskEnvironment env_;

  // Our thread is the mcvd thread.  This is the task runner for the gpu thread.
  scoped_refptr<base::TestSimpleTaskRunner> gpu_task_runner_;
};

TEST_F(CodecImageGroupTest, GroupRegistersForOverlayDestruction) {
  // When we provide an image group with an overlay, it should register for
  // destruction on that overlay.
  Record rec = CreateImageGroup();
  // Note that we don't run any thread loop to completion -- it should assign
  // the callback on our thread, since that's where the overlay is used.
  // We verify expectations now, so that it doesn't matter if any task runners
  // run during teardown.  I'm not sure if the expectations would be checked
  // before or after that.  If after, then posting would still pass, which we
  // don't want.
  testing::Mock::VerifyAndClearExpectations(this);

  // There should not be just one ref to the surface bundle; the CodecImageGroup
  // should have one too.
  ASSERT_FALSE(rec.surface_bundle->HasOneRef());
}

TEST_F(CodecImageGroupTest, SurfaceBundleWithoutOverlayDoesntCrash) {
  // Make sure that it's okay not to have an overlay.  CodecImageGroup should
  // handle ST surface bundles without crashing.
  scoped_refptr<CodecSurfaceBundle> surface_bundle =
      base::MakeRefCounted<CodecSurfaceBundle>();
  scoped_refptr<CodecImageGroup> image_group =
      base::MakeRefCounted<CodecImageGroup>(
          gpu_task_runner_, surface_bundle,
          features::NeedThreadSafeAndroidMedia()
              ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
              : nullptr);
  // TODO(liberato): we should also make sure that adding an image doesn't call
  // ReleaseCodecBuffer when it's added.
}

TEST_F(CodecImageGroupTest, ImagesRetainRefToGroup) {
  // Make sure that keeping an image around is sufficient to keep the group.
  Record rec = CreateImageGroup();
  bool was_destroyed = false;
  rec.image_group->SetDestructionCallback(
      base::BindOnce([](bool* flag) -> void { *flag = true; }, &was_destroyed));
  scoped_refptr<CodecImage> image = new MockCodecImage(kMockImageSize);
  // We're supposed to call this from |gpu_task_runner_|, but all
  // CodecImageGroup really cares about is being single sequence.
  rec.image_group->AddCodecImage(image.get());

  // The image should be sufficient to prevent destruction.
  rec.image_group = nullptr;
  ASSERT_FALSE(was_destroyed);

  // The image should be the last ref to the image group.
  image = nullptr;
  ASSERT_TRUE(was_destroyed);
}

TEST_F(CodecImageGroupTest, ImageGroupDropsForwardsSurfaceDestruction) {
  // CodecImageGroup should notify all images when the surface is destroyed.  We
  // also verify that the image group drops its ref to the surface bundle, so
  // that it doesn't prevent destruction of the overlay that provided it.
  Record rec = CreateImageGroup();
  scoped_refptr<MockCodecImage> image_1 = new MockCodecImage(kMockImageSize);
  scoped_refptr<MockCodecImage> image_2 = new MockCodecImage(kMockImageSize);
  rec.image_group->AddCodecImage(image_1.get());
  rec.image_group->AddCodecImage(image_2.get());

  // Destroy the surface.  All destruction messages should be posted to the
  // gpu thread.
  EXPECT_CALL(*image_1.get(), ReleaseCodecBuffer()).Times(0);
  EXPECT_CALL(*image_2.get(), ReleaseCodecBuffer()).Times(0);
  // Note that we're calling this on the wrong thread, but that's okay.
  rec.overlay()->OnSurfaceDestroyed();
  env_.RunUntilIdle();
  // Run the main loop and guarantee that nothing has run.  It should be posted
  // to |gpu_task_runner_|.
  testing::Mock::VerifyAndClearExpectations(this);

  // Now run |gpu_task_runner_| and verify that the callbacks run.
  EXPECT_CALL(*image_1.get(), ReleaseCodecBuffer()).Times(1);
  EXPECT_CALL(*image_2.get(), ReleaseCodecBuffer()).Times(1);
  gpu_task_runner_->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // The image group should drop its ref to the surface bundle.
  ASSERT_TRUE(rec.surface_bundle->HasOneRef());
}

}  // namespace media
