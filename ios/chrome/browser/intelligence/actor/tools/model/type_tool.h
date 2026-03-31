// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"

class ProfileIOS;
class TypeToolJavaScriptFeature;

namespace web {
class WebState;
}  // namespace web

// Tool to type text into an element on a page.
class TypeTool : public WebActorTool {
 public:
  ~TypeTool() override;

  static base::expected<std::unique_ptr<TypeTool>, ActorToolError> Create(
      const optimization_guide::proto::TypeAction& action,
      ProfileIOS* profile);

  // ActorTool:
  void Execute(ActorCallback callback) override;

 private:
  TypeTool(const optimization_guide::proto::TypeAction& action,
           base::WeakPtr<web::WebState> web_state);

  void OnTargetFrameResolved(
      optimization_guide::proto::TypeAction action,
      ActorCallback callback,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ActorToolError> result);

  optimization_guide::proto::TypeAction action_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<TypeToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<TypeTool> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_
