// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"

#import "base/types/expected.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

FakeToolDelegate::FakeToolDelegate() = default;
FakeToolDelegate::~FakeToolDelegate() = default;

ActorTaskId FakeToolDelegate::GetTaskId() const {
  return ActorTaskId(1);
}

WebStateList* FakeToolDelegate::GetWebStateListForWindowId(int32_t window_id) {
  auto it = web_state_lists_.find(window_id);
  if (it == web_state_lists_.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool FakeToolDelegate::IsWindowIdValid(int32_t window_id) {
  return GetWebStateListForWindowId(window_id) != nullptr;
}

web::WebState* FakeToolDelegate::InsertWebState(
    int32_t window_id,
    const web::NavigationManager::WebLoadParams& load_params,
    bool in_background) {
  if (fail_insert_web_state_) {
    return nullptr;
  }
  WebStateList* web_state_list = GetWebStateListForWindowId(window_id);
  if (!web_state_list) {
    return nullptr;
  }
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(load_params.url);
  web::WebState* web_state_ptr = web_state.get();

  WebStateList::InsertionParams params =
      WebStateList::InsertionParams::Automatic();
  int active_index = web_state_list->active_index();
  if (active_index != WebStateList::kInvalidIndex) {
    params = WebStateList::InsertionParams::AtIndex(active_index + 1);
  }
  bool should_activate = !in_background || web_state_list->empty();
  params.Activate(should_activate);

  web_state_list->InsertWebState(std::move(web_state), params);
  return web_state_ptr;
}

AggregatedJournal& FakeToolDelegate::GetJournal() const {
  return *journal_;
}

ActorToolFactory& FakeToolDelegate::GetToolFactory() const {
  return *tool_factory_;
}

ActorTaskFormFillingHandler*
FakeToolDelegate::GetActorTaskFormFillingHandler() {
  return form_filling_handler_.get();
}

void FakeToolDelegate::InterruptFromTool() {}

void FakeToolDelegate::UninterruptFromTool() {}

void FakeToolDelegate::SetWebStateListForWindowId(
    int32_t window_id,
    WebStateList* web_state_list) {
  web_state_lists_[window_id] =
      web_state_list ? web_state_list->AsWeakPtr() : nullptr;
}

}  // namespace actor
