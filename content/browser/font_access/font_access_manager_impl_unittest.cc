// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/font_access/font_access_test_utils.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  explicit FontAccessManagerSync(blink::mojom::FontAccessManager* manager)
      : manager_(manager) {
    DCHECK(manager);
  }
  std::pair<FontEnumerationStatus, base::ReadOnlySharedMemoryRegion>
  EnumerateLocalFonts() {
    std::pair<FontEnumerationStatus, base::ReadOnlySharedMemoryRegion> result;

    base::RunLoop run_loop;
    manager_->EnumerateLocalFonts(base::BindLambdaForTesting(
        [&](FontEnumerationStatus status,
            base::ReadOnlySharedMemoryRegion region) {
          result.first = status;
          result.second = std::move(region);
          run_loop.Quit();
        }));
    run_loop.Run();

    return result;
  }

  std::pair<FontEnumerationStatus, std::vector<blink::mojom::FontMetadataPtr>>
  ChooseLocalFonts(const std::vector<std::string>& selection) {
    std::pair<FontEnumerationStatus, std::vector<blink::mojom::FontMetadataPtr>>
        result;

    base::RunLoop run_loop;
    manager_->ChooseLocalFonts(
        selection,
        base::BindLambdaForTesting(
            [&](FontEnumerationStatus status,
                std::vector<blink::mojom::FontMetadataPtr> font_metadata) {
              result.first = status;
              result.second = std::move(font_metadata);
              run_loop.Quit();
            }));
    run_loop.Run();

    return result;
  }

 private:
  const raw_ptr<blink::mojom::FontAccessManager> manager_;
};

class FontAccessManagerImplTest : public RenderViewHostImplTestHarness {
 public:
  FontAccessManagerImplTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    const int process_id = main_rfh()->GetProcess()->GetID();
    const int routing_id = main_rfh()->GetRoutingID();
    const GlobalRenderFrameHostId frame_id =
        GlobalRenderFrameHostId{process_id, routing_id};

    cache_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache =
        FontEnumerationCache::CreateForTesting(cache_task_runner_,
                                               absl::nullopt);
    manager_impl_ = FontAccessManagerImpl::CreateForTesting(
        std::move(font_enumeration_cache));
    manager_impl_->BindReceiver(kTestOrigin, frame_id,
                                manager_.BindNewPipeAndPassReceiver());
    manager_sync_ = std::make_unique<FontAccessManagerSync>(manager_.get());

    // Set up permission mock.
    TestBrowserContext* browser_context =
        static_cast<TestBrowserContext*>(main_rfh()->GetBrowserContext());
    browser_context->SetPermissionControllerDelegate(
        std::make_unique<TestFontAccessPermissionManager>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(browser_context);
  }

  void TearDown() override {
    // Ensure that the FontEnumerationCache instance is destroyed before the
    // test ends. This avoids ASAN failures.
    manager_sync_ = nullptr;
    manager_.reset();
    manager_impl_ = nullptr;
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
          std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
        }));
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::GRANTED);
  }

  void AutoDenyPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
        }));
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::DENIED);
  }

  void AskGrantPermission() {
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::ASK);
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
        }));
  }

  void AskDenyPermission() {
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::ASK);
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
        }));
  }

  void SetFrameHidden() {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->VisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);
  }

  void SimulateUserActivation() {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->UpdateUserActivationState(
            blink::mojom::UserActivationUpdateType::kNotifyActivation,
            blink::mojom::UserActivationNotificationType::kTest);
  }

 protected:
  const GURL kTestUrl = GURL("https://example.com/font_access");
  const url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  std::unique_ptr<PermissionControllerImpl> permission_controller_;
  std::unique_ptr<FontAccessManagerImpl> manager_impl_;
  mojo::Remote<blink::mojom::FontAccessManager> manager_;
  std::unique_ptr<FontAccessManagerSync> manager_sync_;
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
namespace {

void ValidateFontEnumerationBasic(FontEnumerationStatus status,
                                  base::ReadOnlySharedMemoryRegion region) {
  ASSERT_EQ(status, FontEnumerationStatus::kOk) << "enumeration status is kOk";

  blink::FontEnumerationTable table;
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  table.ParseFromArray(mapping.memory(), mapping.size());

  blink::FontEnumerationTable_FontMetadata previous_font;
  for (const auto& font : table.fonts()) {
    EXPECT_GT(font.postscript_name().size(), 0ULL)
        << "postscript_name size is not zero.";
    EXPECT_GT(font.full_name().size(), 0ULL) << "full_name size is not zero.";
    EXPECT_GT(font.family().size(), 0ULL) << "family size is not zero.";
    EXPECT_GE(font.stretch(), 0.5f) << "stretch is in 0.5..2.0.";
    EXPECT_LE(font.stretch(), 2.0f) << "stretch is in 0.5..2.0.";
    EXPECT_GE(font.weight(), 1.f) << "weight is in 1..1000.";
    EXPECT_LE(font.weight(), 1000.f) << "weight is in 1..1000.";

    if (previous_font.IsInitialized()) {
      EXPECT_LT(previous_font.postscript_name(), font.postscript_name())
          << "font list is sorted";
    }

    previous_font = font;
  }
}

}  // namespace

TEST_F(FontAccessManagerImplTest, FailsIfFrameNotInViewport) {
  AutoGrantPermission();
  SetFrameHidden();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kNotVisible);
  EXPECT_FALSE(region.IsValid());
}

TEST_F(FontAccessManagerImplTest, EnumerationConsumesUserActivation) {
  AskGrantPermission();
  SimulateUserActivation();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kOk)
      << "Font Enumeration was successful.";

  AskGrantPermission();
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();
  EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation)
      << "User Activation Required.";
}

TEST_F(FontAccessManagerImplTest, PreviouslyGrantedValidateEnumerationBasic) {
  AutoGrantPermission();
  SimulateUserActivation();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kOk);
  ValidateFontEnumerationBasic(std::move(status), std::move(region));
}

TEST_F(FontAccessManagerImplTest, UserActivationRequiredBeforeGrant) {
  AskGrantPermission();
  SimulateUserActivation();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kOk);
}

TEST_F(FontAccessManagerImplTest, EnumerationFailsIfNoActivation) {
  AskGrantPermission();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation);
}

TEST_F(FontAccessManagerImplTest, PermissionDeniedOnAskErrors) {
  AskDenyPermission();
  SimulateUserActivation();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied);
}

TEST_F(FontAccessManagerImplTest, PermissionPreviouslyDeniedErrors) {
  AutoDenyPermission();
  SimulateUserActivation();

  FontEnumerationStatus status;
  base::ReadOnlySharedMemoryRegion region;
  std::tie(status, region) = manager_sync_->EnumerateLocalFonts();

  EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied);
}

TEST_F(FontAccessManagerImplTest, FontAccessContextFindAllFontsTest) {
  FontAccessContext* font_access_context =
      static_cast<FontAccessContext*>(manager_impl_.get());

  FontEnumerationStatus status;
  std::vector<blink::mojom::FontMetadata> fonts;

  base::RunLoop run_loop;
  font_access_context->FindAllFonts(base::BindLambdaForTesting(
      [&](FontEnumerationStatus find_status,
          std::vector<blink::mojom::FontMetadata> find_fonts) {
        status = find_status;
        fonts = std::move(find_fonts);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(status, FontEnumerationStatus::kOk);
  EXPECT_GT(fonts.size(), 0u)
      << "Enumeration expected to yield at least 1 font";
}

#endif

}  // namespace

}  // namespace content
