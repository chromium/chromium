// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ActorToolFactory;
class AggregatedJournal;
class ProfileIOS;

// Service responsible for handling browser actor requests.
class ActorService : public KeyedService {
 public:
  explicit ActorService(ProfileIOS* profile);
  ActorService(ProfileIOS* profile,
               std::unique_ptr<ActorToolFactory> tool_factory);
  ~ActorService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes the given action.
  void ExecuteAction(const optimization_guide::proto::Action& action,
                     ActorTool::ActorCallback callback);

  // Returns the aggregated journal for this service.
  AggregatedJournal* GetJournal() { return journal_.get(); }

 private:
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ActorToolFactory> tool_factory_;
  std::unique_ptr<AggregatedJournal> journal_;

  base::WeakPtrFactory<ActorService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
