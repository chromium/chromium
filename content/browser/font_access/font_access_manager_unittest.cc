// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/font_access/font_access_test_utils.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FontEnumerationStatus;

namespace content {

namespace {

// Synchronous proxy to a blink::mojom::FontAccessManager.
class FontAccessManagerSync {
 public:
  explicit FontAccessManagerSync(
      blink::mojom::FontAccessManager* manager_remote)
      : manager_remote_(manager_remote) {
    DCHECK(manager_remote);
  }
  std::pair<FontEnumerationStatus, base::ReadOnlySharedMemoryRegion>
  EnumerateLocalFonts() {
    std::pair<FontEnumerationStatus, base::ReadOnlySharedMemoryRegion> result;

    base::RunLoop run_loop;
    manager_remote_->EnumerateLocalFonts(base::BindLambdaForTesting(
        [&](FontEnumerationStatus status,
            base::ReadOnlySharedMemoryRegion region) {
          result.first = status;
          result.second = std::move(region);
          run_loop.Quit();
        }));
    run_loop.Run();

    return result;
  }

 private:
  const raw_ptr<blink::mojom::FontAccessManager> manager_remote_;
};

class FontAccessManagerTest : public RenderViewHostImplTestHarness {
 public:
  FontAccessManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    const int process_id = main_rfh()->GetProcess()->GetID();
    const int routing_id = main_rfh()->GetRoutingID();
    const GlobalRenderFrameHostId main_frame_id(process_id, routing_id);

    cache_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache =
        FontEnumerationCache::CreateForTesting(
            cache_task_runner_, FontEnumerationDataSource::Create(),
            /* locale_override= */ std::nullopt);
    manager_ =
        FontAccessManager::CreateForTesting(std::move(font_enumeration_cache));
    manager_->BindReceiver(main_frame_id,
                           manager_remote_.BindNewPipeAndPassReceiver());
    manager_sync_ =
        std::make_unique<FontAccessManagerSync>(manager_remote_.get());

    // Set up permission mock.
    TestBrowserContext* browser_context =
        static_cast<TestBrowserContext*>(main_rfh()->GetBrowserContext());
    browser_context->SetPermissionControllerDelegate(
        std::make_unique<TestFontAccessPermissionManager>());
  }

  void TearDown() override {
    // Ensure that the FontEnumerationCache instance is destroyed before the
    // test ends. This avoids ASAN failures.
    manager_sync_ = nullptr;
    manager_remote_.reset();
    manager_ = nullptr;
    base::RunLoop run_loop;
    cache_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    RenderViewHostImplTestHarness::TearDown();
  }

  TestFontAccessPermissionManager* test_permission_manager() {
    return static_cast<TestFontAccessPermissionManager*>(
        main_rfh()->GetBrowserContext()->GetPermissionControllerDelegate());
  }

  void AutoGrantPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run({blink::mojom::PermissionStatus::GRANTED});
        }));
    test_permission_manager()->SetPermissionStatusForCurrentDocument(
        blink::mojom::PermissionStatus::GRANTED);
  }

  void AutoDenyPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run({blink::mojom::PermissionStatus::DENIED});
        }));
    test_permission_manager()->SetPermissionStatusForCurrentDocument(
        blink::mojom::PermissionStatus::DENIED);
  }

  void AskGrantPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run({blink::mojom::PermissionStatus::GRANTED});
        }));
    test_permission_manager()->SetPermissionStatusForCurrentDocument(
        blink::mojom::PermissionStatus::ASK);
  }

  void AskDenyPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run({blink::mojom::PermissionStatus::DENIED});
        }));
    test_permission_manager()->SetPermissionStatusForCurrentDocument(
        blink::mojom::PermissionStatus::ASK);
  }

  void SetFrameHidden() { test_rvh()->SimulateWasHidden(); }

  void SimulateUserActivation() { main_test_rfh()->SimulateUserActivation(); }

 protected:
  const GURL kTestUrl = GURL("https://example.com/font_access");
  const url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  std::unique_ptr<FontAccessManager> manager_;
  mojo::Remote<blink::mojom::FontAccessManager> manager_remote_;
  std::unique_ptr<FontAccessManagerSync> manager_sync_;
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

void ValidateFontEnumerationBasic(FontEnumerationStatus status,
                                  base::ReadOnlySharedMemoryRegion region) {
  ASSERT_EQ(status, FontEnumerationStatus::kOk) << "enumeration status is kOk";

  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  blink::FontEnumerationTable table;
  EXPECT_TRUE(table.ParseFromArray(mapping.memory(), mapping.size()));

  blink::FontEnumerationTable_FontData previous_font;
  for (const auto& font : table.fonts()) {
    EXPECT_GT(font.postscript_name().size(), 0ULL)
        << "postscript_name size is not zero.";
    EXPECT_GT(font.full_name().size(), 0ULL) << "full_name size is not zero.";
    EXPECT_GT(font.family().size(), 0ULL) << "family size is not zero.";
    EXPECT_GT(font.style().size(), 0ULL) << "style size is not zero.";

    if (previous_font.IsInitialized()) {
      EXPECT_LT(previous_font.postscript_name(), font.postscript_name())
          << "font list is sorted";
    }

    previous_font = font;
  }
}

}  // namespace

TEST_F(FontAccessManagerTest, FailsIfFrameNotInViewport) {
  AutoGrantPermission();
  SetFrameHidden();

  const auto [status, region] = manager_sync_->EnumerateLocalFonts();
  EXPECT_EQ(status, FontEnumerationStatus::kNotVisible);
  EXPECT_FALSE(region.IsValid());
}

TEST_F(FontAccessManagerTest, EnumerationConsumesUserActivation) {
  AskGrantPermission();
  SimulateUserActivation();

  {
    const auto [status, region] = manager_sync_->EnumerateLocalFonts();
    if (FontEnumerationDataSource::IsOsSupported()) {
      EXPECT_EQ(status, FontEnumerationStatus::kOk)
          << "Font Enumeration was successful.";
    } else {
      EXPECT_EQ(status, FontEnumerationStatus::kUnimplemented);
    }
  }

  AskGrantPermission();
  {
    const auto [status, region] = manager_sync_->EnumerateLocalFonts();
    EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation)
        << "User Activation Required.";
  }
}

TEST_F(FontAccessManagerTest, PreviouslyGrantedValidateEnumerationBasic) {
  AutoGrantPermission();
  SimulateUserActivation();

  auto [status, region] = manager_sync_->EnumerateLocalFonts();
  if (FontEnumerationDataSource::IsOsSupported()) {
    EXPECT_EQ(status, FontEnumerationStatus::kOk);
    ValidateFontEnumerationBasic(std::move(status), std::move(region));
  } else {
    EXPECT_EQ(status, FontEnumerationStatus::kUnimplemented);
  }
}

TEST_F(FontAccessManagerTest, UserActivationRequiredBeforeGrant) {
  AskGrantPermission();
  SimulateUserActivation();

  const auto [status, region] = manager_sync_->EnumerateLocalFonts();
  if (FontEnumerationDataSource::IsOsSupported()) {
    EXPECT_EQ(status, FontEnumerationStatus::kOk);
  } else {
    EXPECT_EQ(status, FontEnumerationStatus::kUnimplemented);
  }
}

TEST_F(FontAccessManagerTest, EnumerationFailsIfNoActivation) {
  AskGrantPermission();

  const auto [status, region] = manager_sync_->EnumerateLocalFonts();
  EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation);
}

TEST_F(FontAccessManagerTest, PermissionDeniedOnAskErrors) {
  AskDenyPermission();
  SimulateUserActivation();

  const auto [status, region] = manager_sync_->EnumerateLocalFonts();
  EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied);
}

TEST_F(FontAccessManagerTest, PermissionPreviouslyDeniedErrors) {
  AutoDenyPermission();
  SimulateUserActivation();

  const auto [status, region] = manager_sync_->EnumerateLocalFonts();
  EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied);
}

}  // namespace

}  // namespace content
