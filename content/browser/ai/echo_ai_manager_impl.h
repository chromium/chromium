// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
#define CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_

#include <variant>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIManager` that creates session which
// only echoes back the prompt text used for testing, and all the parameters
// will be set using the default value.
class EchoAIManagerImpl : public blink::mojom::AIManager {
 public:
  // The context size for EchoAIManagerImpl is intentionally set to a small
  // value so we can easily simulate the context overflow scenario.
  static constexpr int kMaxContextSizeInTokens = 1000;

  EchoAIManagerImpl(const EchoAIManagerImpl&) = delete;
  EchoAIManagerImpl& operator=(const EchoAIManagerImpl&) = delete;

  ~EchoAIManagerImpl() override;

  static void Create(mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  friend base::NoDestructor<EchoAIManagerImpl>;

  EchoAIManagerImpl();

  // `blink::mojom::AIManager` implementation.
  void CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptionsPtr options,
      CanCreateLanguageModelCallback callback) override;
  void CreateLanguageModel(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      blink::mojom::AILanguageModelCreateOptionsPtr options) override;
  void CanCreateSummarizer(blink::mojom::AISummarizerCreateOptionsPtr options,
                           CanCreateSummarizerCallback callback) override;
  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;
  void GetLanguageModelParams(GetLanguageModelParamsCallback callback) override;
  void CanCreateWriter(blink::mojom::AIWriterCreateOptionsPtr options,
                       CanCreateWriterCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CanCreateRewriter(blink::mojom::AIRewriterCreateOptionsPtr options,
                         CanCreateRewriterCallback callback) override;
  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) override;
  void CanCreateProofreader(blink::mojom::AIProofreaderCreateOptionsPtr options,
                            CanCreateProofreaderCallback callback) override;
  void CreateProofreader(
      mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient>
          client,
      blink::mojom::AIProofreaderCreateOptionsPtr options) override;
  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote) override;

  template <typename CanCreateCallback>
  void CanCreateClient(CanCreateCallback callback);

  template <typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void CreateClient(mojo::Remote<AIClientRemote> client_remote);

  template <typename AICreateOptions, typename CanCreateCallback>
  void CanCreateWritingAssistanceClient(AICreateOptions options,
                                        CanCreateCallback callback);

  template <typename AICreateOptions,
            typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void CreateWritingAssistanceClient(mojo::PendingRemote<AIClientRemote> client,
                                     AICreateOptions options);

  template <typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void ReturnAIClientCreationResult(mojo::Remote<AIClientRemote> client_remote);

  void ReturnAILanguageModelCreationResult(
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote,
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      base::flat_set<blink::mojom::AILanguageModelPromptType>
          enabled_input_types,
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      uint32_t initial_input_usage);

  void DoMockDownloadingAndReturn(base::OnceClosure callback);

  // Returns whether the current mojo receiver triggered mock model download.
  bool IsModelDownloadedForCurrentReciever() const;

  // The set of mojo receivers that have triggered mock model download.
  base::flat_set<mojo::ReceiverId> model_downloaded_receivers_;

  mojo::RemoteSet<blink::mojom::ModelDownloadProgressObserver>
      download_progress_observers_;

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;

  base::WeakPtrFactory<EchoAIManagerImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
