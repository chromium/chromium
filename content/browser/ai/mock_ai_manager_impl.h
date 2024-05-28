// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_MOCK_AI_MANAGER_IMPL_H_
#define CONTENT_BROWSER_AI_MOCK_AI_MANAGER_IMPL_H_

#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

namespace content {

// The mock implementation of `blink::mojom::AIManager` used for testing.
class MockAIManagerImpl : public content::DocumentUserData<MockAIManagerImpl>,
                          public blink::mojom::AIManager {
 public:
  MockAIManagerImpl(const MockAIManagerImpl&) = delete;
  MockAIManagerImpl& operator=(const MockAIManagerImpl&) = delete;

  ~MockAIManagerImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  friend class DocumentUserData<MockAIManagerImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit MockAIManagerImpl(content::RenderFrameHost* rfh);

  // `blink::mojom::AIManager` implementation.
  void CanCreateTextSession(CanCreateTextSessionCallback callback) override;

  void CreateTextSession(
      mojo::PendingReceiver<::blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      CreateTextSessionCallback callback) override;

  void GetDefaultTextSessionSamplingParams(
      GetDefaultTextSessionSamplingParamsCallback callback) override;

  mojo::Receiver<blink::mojom::AIManager> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_MOCK_AI_MANAGER_IMPL_H_
