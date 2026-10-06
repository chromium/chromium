// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_decision_model.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"

namespace content {

namespace {

std::vector<std::string> GetLabels(
    const blink::mojom::AIDecisionModelQuestion& question) {
  switch (question.type) {
    case blink::mojom::AIDecisionModelQuestionType::kBoolean:
      return {"true", "false"};
    case blink::mojom::AIDecisionModelQuestionType::kChoice: {
      std::vector<std::string> labels;
      labels.reserve(question.options.size());
      for (const auto& option : question.options) {
        if (option) {
          labels.push_back(option->label);
        }
      }
      return labels;
    }
    case blink::mojom::AIDecisionModelQuestionType::kScore: {
      if (question.options.empty()) {
        return {"1", "2", "3", "4", "5"};
      }
      std::vector<std::string> labels;
      labels.reserve(question.options.size());
      for (const auto& option : question.options) {
        if (option) {
          labels.push_back(option->label);
        }
      }
      return labels;
    }
  }
}

}  // namespace

EchoAIDecisionModel::EchoAIDecisionModel(
    std::vector<blink::mojom::AIDecisionModelQuestionPtr> questions)
    : questions_(std::move(questions)) {}

EchoAIDecisionModel::~EchoAIDecisionModel() = default;

void EchoAIDecisionModel::Decide(const std::string& input,
                                 DecideCallback callback) {
  if (input.empty()) {
    std::move(callback).Run(
        base::unexpected(blink::mojom::AIDecisionModelDecideError::New(
            blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest,
            /*quota_error_info=*/nullptr)));
    return;
  }

  auto result = blink::mojom::AIDecisionModelResult::New();
  for (const auto& q : questions_) {
    if (!q) {
      continue;
    }
    const std::vector<std::string> labels = GetLabels(*q);
    if (labels.empty()) {
      std::move(callback).Run(
          base::unexpected(blink::mojom::AIDecisionModelDecideError::New(
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest,
              /*quota_error_info=*/nullptr)));
      return;
    }

    auto decision = blink::mojom::AIDecisionModelDecision::New();
    decision->question_id = q->id;

    switch (q->type) {
      case blink::mojom::AIDecisionModelQuestionType::kBoolean: {
        decision->top_label = "false";
        decision->confidence = 0.85f;
        decision->probabilities.push_back(
            blink::mojom::AIDecisionModelOptionProbability::New("true", 0.15f));
        decision->probabilities.push_back(
            blink::mojom::AIDecisionModelOptionProbability::New("false",
                                                                0.85f));
        break;
      }
      case blink::mojom::AIDecisionModelQuestionType::kChoice: {
        const size_t n = labels.size();
        const float top_prob = (n == 1) ? 1.0f : 0.70f;
        const float rem_prob =
            (n > 1) ? (1.0f - top_prob) / static_cast<float>(n - 1) : 0.0f;
        decision->top_label = labels[0];
        decision->confidence = top_prob;
        for (size_t i = 0; i < n; ++i) {
          decision->probabilities.push_back(
              blink::mojom::AIDecisionModelOptionProbability::New(
                  labels[i], i == 0 ? top_prob : rem_prob));
        }
        break;
      }
      case blink::mojom::AIDecisionModelQuestionType::kScore: {
        const size_t n = labels.size();
        const float top_prob = (n == 1) ? 1.0f : 0.60f;
        const float rem_prob =
            (n > 1) ? (1.0f - top_prob) / static_cast<float>(n - 1) : 0.0f;
        decision->top_label = labels.back();
        decision->confidence = top_prob;

        bool all_numeric = true;
        std::vector<double> numeric_values(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
          if (!base::StringToDouble(labels[i], &numeric_values[i])) {
            all_numeric = false;
            break;
          }
        }

        float expected_score = 0.0f;
        for (size_t i = 0; i < n; ++i) {
          const float p = (i + 1 == n) ? top_prob : rem_prob;
          const float val = all_numeric ? static_cast<float>(numeric_values[i])
                                        : static_cast<float>(i + 1);
          expected_score += val * p;
          decision->probabilities.push_back(
              blink::mojom::AIDecisionModelOptionProbability::New(labels[i],
                                                                  p));
        }
        decision->score = expected_score;
        break;
      }
    }

    result->decisions.push_back(std::move(decision));
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace content
