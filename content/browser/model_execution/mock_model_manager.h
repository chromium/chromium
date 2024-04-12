// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_MANAGER_H_
#define CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_MANAGER_H_

#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom.h"

// The mock implementation of `blink::mojom::ModelManager` used for testing.
class MockModelManager : public content::DocumentUserData<MockModelManager>,
                         public blink::mojom::ModelManager {
 public:
  MockModelManager(const MockModelManager&) = delete;
  MockModelManager& operator=(const MockModelManager&) = delete;

  ~MockModelManager() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ModelManager> receiver);

 private:
  friend class DocumentUserData<MockModelManager>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit MockModelManager(content::RenderFrameHost* rfh);

  // `blink::mojom::ModelManager` implementation.
  void CanCreateGenericSession(
      CanCreateGenericSessionCallback callback) override;

  void CreateGenericSession(
      mojo::PendingReceiver<::blink::mojom::ModelGenericSession> receiver,
      blink::mojom::ModelGenericSessionSamplingParamsPtr sampling_params,
      CreateGenericSessionCallback callback) override;

  void GetDefaultGenericSessionSamplingParams(
      GetDefaultGenericSessionSamplingParamsCallback callback) override;

  mojo::Receiver<blink::mojom::ModelManager> receiver_{this};
};

#endif  // CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_MANAGER_H_
