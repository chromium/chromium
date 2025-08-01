// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_BACKEND_SESSION_H_
#define SERVICES_ON_DEVICE_MODEL_BACKEND_SESSION_H_

#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

// An interface for a session that can be implemented by different backends.
class COMPONENT_EXPORT(ON_DEVICE_MODEL) BackendSession {
 public:
  virtual ~BackendSession() = default;

  // Appends input to a session. Calls `on_complete` when the append operation
  // has completed.
  virtual void Append(
      on_device_model::mojom::AppendOptionsPtr options,
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
      base::OnceClosure on_complete) = 0;

  // Generates output for this session. Calls `on_complete` when all output has
  // completed.
  virtual void Generate(
      on_device_model::mojom::GenerateOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
      base::OnceClosure on_complete) = 0;

  // Calls `callback` for the size of `input` in tokens.
  virtual void SizeInTokens(on_device_model::mojom::InputPtr input,
                            base::OnceCallback<void(uint32_t)> callback) = 0;

  // Gets the probability score of the first token in `text` on top of the
  // current context.
  virtual void Score(const std::string& text,
                     base::OnceCallback<void(float)> callback) = 0;

  // Gets the probability for a series of tokens on top of the current
  // context.
  virtual void GetProbabilitiesBlocking(
      const std::string& input,
      base::OnceCallback<void(const std::vector<float>&)> callback) = 0;

  // Clones the current session. The cloned session will have the same context
  // as the current session.
  virtual std::unique_ptr<BackendSession> Clone() = 0;

  // Start a new Automatic Speech Recognition transcription stream.
  virtual void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder>
          responder) = 0;

  // Add a chunk of audio the ASR stream.
  virtual void AsrAddAudioChunk(mojom::AudioDataPtr data) = 0;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_BACKEND_SESSION_H_
