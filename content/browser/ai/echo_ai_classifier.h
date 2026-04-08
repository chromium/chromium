// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_CLASSIFIER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_CLASSIFIER_H_

#include "third_party/blink/public/mojom/ai/ai_classifier.mojom.h"

namespace content {

class EchoAIClassifier : public blink::mojom::AIClassifier {
 public:
  EchoAIClassifier();
  EchoAIClassifier(const EchoAIClassifier&) = delete;
  EchoAIClassifier& operator=(const EchoAIClassifier&) = delete;
  ~EchoAIClassifier() override;

  // blink::mojom::AIClassifier:
  void Classify(const std::string& input,
                const std::string& context,
                mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                    pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_CLASSIFIER_H_
