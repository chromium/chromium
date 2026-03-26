// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_HISTORY_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_HISTORY_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

struct ActuationError;
class ProfileIOS;

namespace optimization_guide::proto {
class HistoryBackAction;
class HistoryForwardAction;
}  // namespace optimization_guide::proto

namespace web {
class WebState;
}  // namespace web

// Actuation tool to navigate back or forward in a tab's history.
class HistoryTool : public ActuationTool {
 public:
  ~HistoryTool() override;

  // Create the actuation tool to handle "go back" action.
  static base::expected<std::unique_ptr<HistoryTool>, ActuationError> Create(
      const optimization_guide::proto::HistoryBackAction& action,
      ProfileIOS* profile);

  // Create the actuation tool to handle "go forward" action.
  static base::expected<std::unique_ptr<HistoryTool>, ActuationError> Create(
      const optimization_guide::proto::HistoryForwardAction& action,
      ProfileIOS* profile);

  // ActuationTool:
  void Execute(ActuationCallback callback) override;

 private:
  // Internal helper to create the public `Create` method.
  template <typename HistoryAction>
  static base::expected<std::unique_ptr<HistoryTool>, ActuationError>
  CreateInternal(const HistoryAction& action, ProfileIOS* profile);

  HistoryTool(bool is_back_action, base::WeakPtr<web::WebState> web_state);

  bool is_back_action_;
  base::WeakPtr<web::WebState> web_state_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_HISTORY_TOOL_H_
