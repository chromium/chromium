// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/audio_input_ipc_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_source_parameters.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/modules/media/audio/mojo_audio_input_ipc.h"

namespace blink {

namespace {

void CreateMojoAudioInputStreamOnMainThread(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& source_params,
    mojo::PendingRemote<mojom::blink::RendererAudioInputStreamFactoryClient>
        client,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t total_segments) {
  if (auto* web_frame = static_cast<WebLocalFrame*>(
          blink::WebFrame::FromFrameToken(frame_token))) {
    web_frame->Client()->CreateAudioInputStream(
        std::move(client), source_params.session_id, params,
        automatic_gain_control, total_segments);
  }
}

void CreateMojoAudioInputStream(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& source_params,
    mojo::PendingRemote<mojom::blink::RendererAudioInputStreamFactoryClient>
        client,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32_t total_segments) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateMojoAudioInputStreamOnMainThread, frame_token,
                     source_params, std::move(client), params,
                     automatic_gain_control, total_segments));
}

void AssociateInputAndOutputForAec(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  auto task = base::BindOnce(
      [](const blink::LocalFrameToken& frame_token,
         const base::UnguessableToken& input_stream_id,
         const std::string& output_device_id) {
        if (auto* web_frame = static_cast<WebLocalFrame*>(
                WebFrame::FromFrameToken(frame_token))) {
          web_frame->Client()->AssociateInputAndOutputForAec(input_stream_id,
                                                             output_device_id);
        }
      },
      frame_token, input_stream_id, output_device_id);
  main_task_runner->PostTask(FROM_HERE, std::move(task));
}
}  // namespace

AudioInputIPCFactory* AudioInputIPCFactory::instance_ = nullptr;

AudioInputIPCFactory::AudioInputIPCFactory(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK(!instance_);
  instance_ = this;
}

AudioInputIPCFactory::~AudioInputIPCFactory() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

std::unique_ptr<media::AudioInputIPC> AudioInputIPCFactory::CreateAudioInputIPC(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& source_params) const {
  CHECK(!source_params.session_id.is_empty());
  return std::make_unique<MojoAudioInputIPC>(
      source_params,
      base::BindRepeating(&CreateMojoAudioInputStream, main_task_runner_,
                          frame_token),
      base::BindRepeating(&AssociateInputAndOutputForAec, main_task_runner_,
                          frame_token));
}

}  // namespace blink
