// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
#define CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_

#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIManager` that creates session which
// only echoes back the prompt text used for testing.
class EchoAIManagerImpl : public blink::mojom::AIManager {
 public:
  EchoAIManagerImpl(const EchoAIManagerImpl&) = delete;
  EchoAIManagerImpl& operator=(const EchoAIManagerImpl&) = delete;

  ~EchoAIManagerImpl() override;

  static void Create(content::BrowserContext* browser_context,
                     mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  friend base::NoDestructor<EchoAIManagerImpl>;

  explicit EchoAIManagerImpl(content::BrowserContext* browser_context);

  // `blink::mojom::AIManager` implementation.
  void CanCreateTextSession(CanCreateTextSessionCallback callback) override;

  void CreateTextSession(
      mojo::PendingReceiver<::blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      CreateTextSessionCallback callback) override;

  void GetDefaultTextSessionSamplingParams(
      GetDefaultTextSessionSamplingParamsCallback callback) override;

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
