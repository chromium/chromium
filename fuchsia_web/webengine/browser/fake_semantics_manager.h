// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTICS_MANAGER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTICS_MANAGER_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/accessibility/semantics/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include <optional>

#include "base/functional/callback.h"
#include "fuchsia_web/webengine/browser/fake_semantic_tree.h"

class FakeSemanticsManager : public fuchsia::accessibility::semantics::testing::
                                 SemanticsManager_TestBase {
 public:
  FakeSemanticsManager();
  ~FakeSemanticsManager() override;

  FakeSemanticsManager(const FakeSemanticsManager&) = delete;
  FakeSemanticsManager& operator=(const FakeSemanticsManager&) = delete;

  bool is_view_registered() const { return view_ref_.reference.is_valid(); }
  bool is_listener_valid() const { return static_cast<bool>(listener_); }
  FakeSemanticTree* semantic_tree() { return &semantic_tree_; }
  int32_t num_actions_handled() { return num_actions_handled_; }
  int32_t num_actions_unhandled() { return num_actions_unhandled_; }

  // Directly call the listener to simulate Fuchsia setting the semantics mode.
  void SetSemanticsModeEnabled(bool is_enabled);

  // Pumps the message loop until the RegisterViewForSemantics() is called.
  void WaitUntilViewRegistered();

  // The value returned by hit testing is written to a class member. In the case
  // Run() times out, the function continues so we don't want to write to a
  // local variable.
  uint32_t HitTestAtPointSync(fuchsia::math::PointF target_point);

  // A helper function for RequestAccessibilityAction.
  void CheckNumActions();

  // TODO(crbug.com/40212707): Remove async RequestAccessibilityAction(), and
  // replace with RequestAccessibilityActionSync().
  // Request the client to perform |action| on the node with |node_id|.
  void RequestAccessibilityAction(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action);

  // Request the client to perform |action| on the node with |node_id|.
  bool RequestAccessibilityActionSync(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action);

  // Runs until |num_actions| accessibility actions have been handled.
  void RunUntilNumActionsHandledEquals(int32_t num_actions);

  // fuchsia::accessibility::semantics::SemanticsManager implementation.
  void RegisterViewForSemantics(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<fuchsia::accessibility::semantics::SemanticListener>
          listener,
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          semantic_tree_request) final;

  void NotImplemented_(const std::string& name) final;

 private:
  fuchsia::ui::views::ViewRef view_ref_;
  fuchsia::accessibility::semantics::SemanticListenerPtr listener_;

  // This fake only supports one SemanticTree, unlike the real SemanticsManager
  // which can support many.
  FakeSemanticTree semantic_tree_;

  std::optional<uint32_t> hit_test_result_;
  int32_t num_actions_handled_ = 0;
  int32_t num_actions_unhandled_ = 0;
  int32_t expected_num_actions_ = 0;
  base::RepeatingClosure on_expected_num_actions_;
  base::OnceClosure on_view_registered_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTICS_MANAGER_H_
