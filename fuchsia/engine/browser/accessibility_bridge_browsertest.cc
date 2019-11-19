// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/accessibility/semantics/cpp/fidl_test_base.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <zircon/types.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::accessibility::semantics::Node;
using fuchsia::accessibility::semantics::SemanticListener;
using fuchsia::accessibility::semantics::SemanticsManager;
using fuchsia::accessibility::semantics::SemanticTree;

namespace {

const char kPage1Path[] = "/ax1.html";
const char kPage2Path[] = "/batching.html";
const char kPage1Title[] = "accessibility 1";
const char kPage2Title[] = "lots of nodes!";
const char kButtonName[] = "a button";
const char kNodeName[] = "last node";
const char kParagraphName[] = "a third paragraph";
const size_t kPage1NodeCount = 5;
const size_t kPage2NodeCount = 200;

class FakeSemanticTree
    : public fuchsia::accessibility::semantics::testing::SemanticTree_TestBase {
 public:
  FakeSemanticTree() = default;
  ~FakeSemanticTree() override = default;

  // fuchsia::accessibility::semantics::SemanticTree implementation.
  void UpdateSemanticNodes(std::vector<Node> nodes) final {
    for (auto& node : nodes)
      nodes_.push_back(std::move(node));
  }

  void DeleteSemanticNodes(std::vector<uint32_t> node_ids) final {
    for (auto id : node_ids) {
      for (uint i = 0; i < nodes_.size(); i++) {
        if (nodes_.at(i).node_id() == id)
          nodes_.erase(nodes_.begin() + i);
      }
    }
  }

  void CommitUpdates(CommitUpdatesCallback callback) final {
    callback();
    if (on_commit_updates_)
      std::move(on_commit_updates_).Run();
  }

  void NotImplemented_(const std::string& name) final {
    NOTIMPLEMENTED() << name;
  }

  void RunUntilNodeCountAtLeast(size_t count) {
    DCHECK(!on_commit_updates_);

    if (nodes_.size() >= count)
      return;

    // May take multiple commits before node count is sufficient.
    do {
      base::RunLoop run_loop;
      on_commit_updates_ = run_loop.QuitClosure();
      run_loop.Run();
    } while (nodes_.size() < count);
  }

  bool HasNodeWithLabel(base::StringPiece name) {
    for (auto& node : nodes_) {
      if (node.has_attributes() && node.attributes().has_label() &&
          node.attributes().label() == name) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<Node> nodes_;
  base::OnceClosure on_commit_updates_;

  DISALLOW_COPY_AND_ASSIGN(FakeSemanticTree);
};

class FakeSemanticsManager : public fuchsia::accessibility::semantics::testing::
                                 SemanticsManager_TestBase {
 public:
  FakeSemanticsManager() : semantic_tree_binding_(&semantic_tree_) {}
  ~FakeSemanticsManager() override = default;

  bool is_view_registered() const { return view_ref_.reference.is_valid(); }
  bool is_listener_valid() const { return static_cast<bool>(listener_); }
  FakeSemanticTree* semantic_tree() { return &semantic_tree_; }

  // Directly call the listener to simulate Fuchsia setting the semantics mode.
  void SetSemanticsModeEnabled(bool is_enabled) {
    listener_->OnSemanticsModeChanged(is_enabled, []() {});
  }

  // fuchsia::accessibility::semantics::SemanticsManager implementation.
  void RegisterViewForSemantics(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<SemanticListener> listener,
      fidl::InterfaceRequest<SemanticTree> semantic_tree_request) final {
    view_ref_ = std::move(view_ref);
    listener_ = listener.Bind();
    semantic_tree_binding_.Bind(std::move(semantic_tree_request));
  }

  void NotImplemented_(const std::string& name) final {
    NOTIMPLEMENTED() << name;
  }

 private:
  fuchsia::ui::views::ViewRef view_ref_;
  fuchsia::accessibility::semantics::SemanticListenerPtr listener_;
  FakeSemanticTree semantic_tree_;
  fidl::Binding<SemanticTree> semantic_tree_binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeSemanticsManager);
};

}  // namespace

class AccessibilityBridgeTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  AccessibilityBridgeTest() : semantics_manager_binding_(&semantics_manager_) {
    cr_fuchsia::WebEngineBrowserTest::set_test_server_root(
        base::FilePath(cr_fuchsia::kTestServerRoot));
  }

  ~AccessibilityBridgeTest() override = default;

  void SetUpOnMainThread() override {
    fuchsia::accessibility::semantics::SemanticsManagerPtr
        semantics_manager_ptr;
    semantics_manager_binding_.Bind(semantics_manager_ptr.NewRequest());

    frame_ptr_ =
        cr_fuchsia::WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_ptr_);
    frame_impl_->set_semantics_manager_for_test(
        std::move(semantics_manager_ptr));

    // Call CreateView to trigger creation of accessibility bridge.
    auto view_tokens = scenic::NewViewTokenPair();
    frame_ptr_->CreateView(std::move(view_tokens.first));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  fuchsia::web::FramePtr frame_ptr_;
  FrameImpl* frame_impl_;
  FakeSemanticsManager semantics_manager_;
  fidl::Binding<SemanticsManager> semantics_manager_binding_;
  cr_fuchsia::TestNavigationListener navigation_listener_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityBridgeTest);
};

// Test registration to the SemanticsManager and accessibility mode on
// WebContents is set correctly.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, RegisterViewRef) {
  // Check that setup is successful.
  EXPECT_TRUE(semantics_manager_.is_view_registered());
  EXPECT_TRUE(semantics_manager_.is_listener_valid());

  // Change the accessibility mode on the Fuchsia side and check that it is
  // propagated correctly.
  EXPECT_FALSE(frame_impl_->web_contents_for_test()
                   ->IsWebContentsOnlyAccessibilityModeForTesting());
  semantics_manager_.SetSemanticsModeEnabled(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl_->web_contents_for_test()
                  ->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, CorrectDataSent) {
  fuchsia::web::NavigationControllerPtr controller;
  frame_ptr_->GetNavigationController(controller.NewRequest());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));

  semantics_manager_.SetSemanticsModeEnabled(true);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Check that the data values are correct in the FakeSemanticTree.
  // TODO(fxb/18796): Test more fields once Chrome to Fuchsia conversions are
  // available.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kButtonName));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kParagraphName));
}

// Batching is performed when the number of nodes to send or delete exceeds the
// maximum, as set on the Fuchsia side. Check that all nodes are received by the
// Semantic Tree when batching is performed.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DataSentWithBatching) {
  fuchsia::web::NavigationControllerPtr controller;
  frame_ptr_->GetNavigationController(controller.NewRequest());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  semantics_manager_.SetSemanticsModeEnabled(true);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title2, kPage2Title);

  // Run until we expect more than a batch's worth of nodes to be present.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(semantics_manager_.semantic_tree()->HasNodeWithLabel(kNodeName));
}

// Check that semantics information is correctly sent when navigating from page
// to page.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, TestNavigation) {
  fuchsia::web::NavigationControllerPtr controller;
  frame_ptr_->GetNavigationController(controller.NewRequest());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));

  semantics_manager_.SetSemanticsModeEnabled(true);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kButtonName));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->HasNodeWithLabel(kParagraphName));

  GURL title2(embedded_test_server()->GetURL(kPage2Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(semantics_manager_.semantic_tree()->HasNodeWithLabel(kNodeName));
}
