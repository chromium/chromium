// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_DECISION_MODEL_H_
#define CONTENT_BROWSER_AI_ECHO_AI_DECISION_MODEL_H_

#include <string>
#include <vector>

#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

namespace content {

// Deterministic mock implementation of `blink::mojom::AIDecisionModel` used for
// web platform tests when no real on-device model service is bound.
class EchoAIDecisionModel : public blink::mojom::AIDecisionModel {
 public:
  explicit EchoAIDecisionModel(
      std::vector<blink::mojom::AIDecisionModelQuestionPtr> questions);
  EchoAIDecisionModel(const EchoAIDecisionModel&) = delete;
  EchoAIDecisionModel& operator=(const EchoAIDecisionModel&) = delete;
  ~EchoAIDecisionModel() override;

  // blink::mojom::AIDecisionModel:
  void Decide(const std::string& input, DecideCallback callback) override;

 private:
  std::vector<blink::mojom::AIDecisionModelQuestionPtr> questions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_DECISION_MODEL_H_
