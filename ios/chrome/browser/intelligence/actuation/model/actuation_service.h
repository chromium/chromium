// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_

#import <memory>
#import <string>

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ActuationToolFactory;
class ProfileIOS;

// Service responsible for handling browser actuation requests.
class ActuationService : public KeyedService {
 public:
  explicit ActuationService(ProfileIOS* profile);
  ~ActuationService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes the given action.
  void ExecuteAction(const optimization_guide::proto::Action& action,
                     ActuationTool::ActuationCallback callback);

 private:
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ActuationToolFactory> tool_factory_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_
