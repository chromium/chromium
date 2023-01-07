// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/fake_semantics_manager.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"

FakeSemanticsManager::FakeSemanticsManager() = default;

FakeSemanticsManager::~FakeSemanticsManager() = default;

void FakeSemanticsManager::SetSemanticsModeEnabled(bool is_enabled) {
  if (!is_enabled)
    semantic_tree_.Clear();

  listener_->OnSemanticsModeChanged(is_enabled, []() {});
}

void FakeSemanticsManager::WaitUntilViewRegistered() {
  base::RunLoop loop;
  on_view_registered_ = loop.QuitClosure();
  loop.Run();
}

uint32_t FakeSemanticsManager::HitTestAtPointSync(
    fuchsia::math::PointF target_point) {
  hit_test_result_.reset();
  base::RunLoop run_loop;
  listener_->HitTest(target_point,
                     [quit = run_loop.QuitClosure(),
                      this](fuchsia::accessibility::semantics::Hit hit) {
                       if (hit.has_node_id()) {
                         hit_test_result_ = hit.node_id();
                       }
                       quit.Run();
                     });
  run_loop.Run();

  return hit_test_result_.value();
}

void FakeSemanticsManager::CheckNumActions() {
  if (num_actions_handled_ + num_actions_unhandled_ >= expected_num_actions_) {
    DCHECK(on_expected_num_actions_);
    on_expected_num_actions_.Run();
  }
}

bool FakeSemanticsManager::RequestAccessibilityActionSync(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action) {
  base::RunLoop run_loop;
  bool action_handled = false;
  listener_->OnAccessibilityActionRequested(
      node_id, action, [&action_handled, &run_loop](bool handled) {
        action_handled = handled;
        run_loop.QuitClosure().Run();
      });
  run_loop.Run();

  return action_handled;
}

void FakeSemanticsManager::RequestAccessibilityAction(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action) {
  listener_->OnAccessibilityActionRequested(node_id, action,
                                            [this](bool handled) {
                                              if (handled) {
                                                num_actions_handled_++;
                                              } else {
                                                num_actions_unhandled_++;
                                              }
                                              CheckNumActions();
                                            });
}

void FakeSemanticsManager::RunUntilNumActionsHandledEquals(
    int32_t num_actions) {
  DCHECK(!on_expected_num_actions_);
  if (num_actions_handled_ + num_actions_unhandled_ >= num_actions)
    return;

  expected_num_actions_ = num_actions;
  base::RunLoop run_loop;
  on_expected_num_actions_ = run_loop.QuitClosure();
  run_loop.Run();
}

void FakeSemanticsManager::RegisterViewForSemantics(
    fuchsia::ui::views::ViewRef view_ref,
    fidl::InterfaceHandle<fuchsia::accessibility::semantics::SemanticListener>
        listener,
    fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
        semantic_tree_request) {
  view_ref_ = std::move(view_ref);
  listener_ = listener.Bind();
  semantic_tree_.Bind(std::move(semantic_tree_request));
  if (on_view_registered_) {
    std::move(on_view_registered_).Run();
  }
}

void FakeSemanticsManager::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << name;
}
