// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"

#import "base/types/expected.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

FakeToolDelegate::FakeToolDelegate() = default;
FakeToolDelegate::~FakeToolDelegate() = default;

ActorTaskId FakeToolDelegate::GetTaskId() const {
  return ActorTaskId(1);
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

}  // namespace actor
