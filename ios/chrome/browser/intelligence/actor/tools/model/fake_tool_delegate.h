// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_

#import <optional>
#import <utility>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"

namespace actor {

class AggregatedJournal;
class ActorToolFactory;

// A fake implementation of ToolDelegate to use in tests.
class FakeToolDelegate : public ToolDelegate {
 public:
  FakeToolDelegate();
  ~FakeToolDelegate() override;

  // ToolDelegate:
  ActorTaskId GetTaskId() const override;
  AggregatedJournal& GetJournal() const override;
  ActorToolFactory& GetToolFactory() const override;
  ActorTaskFormFillingHandler* GetActorTaskFormFillingHandler() override;
  void InterruptFromTool() override;
  void UninterruptFromTool() override;

  void set_form_filling_handler(
      std::unique_ptr<ActorTaskFormFillingHandler> handler) {
    form_filling_handler_ = std::move(handler);
  }

 private:
  std::unique_ptr<ActorTaskFormFillingHandler> form_filling_handler_;
  raw_ptr<AggregatedJournal> journal_ = nullptr;
  raw_ptr<ActorToolFactory> tool_factory_ = nullptr;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_
