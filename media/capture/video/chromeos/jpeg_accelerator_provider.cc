// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/jpeg_accelerator_provider.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {

JpegAcceleratorProviderImpl::JpegAcceleratorProviderImpl(
    MojoMjpegDecodeAcceleratorFactoryCB jda_factory,
    MojoJpegEncodeAcceleratorFactoryCB jea_factory)
    : ui_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      jda_factory_(std::move(jda_factory)),
      jea_factory_(std::move(jea_factory)) {
  CHECK(ash::mojo_service_manager::IsServiceManagerBound());
  auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
  proxy->Register(
      /*service_name=*/chromeos::mojo_services::kCrosJpegAccelerator,
      provider_receiver_.BindNewPipeAndPassRemote());
}

JpegAcceleratorProviderImpl::~JpegAcceleratorProviderImpl() {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
}

void JpegAcceleratorProviderImpl::GetJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  jea_factory_.Run(std::move(jea_receiver));
}

void JpegAcceleratorProviderImpl::GetMjpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  jda_factory_.Run(std::move(jda_receiver));
}

void JpegAcceleratorProviderImpl::AddReceiver(
    mojo::ScopedMessagePipeHandle message_pipe) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  receiver_set_.Add(this,
                    mojo::PendingReceiver<cros::mojom::JpegAcceleratorProvider>(
                        std::move(message_pipe)));
}

void JpegAcceleratorProviderImpl::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JpegAcceleratorProviderImpl::AddReceiver,
                     weak_factory_.GetWeakPtr(), std::move(receiver)));
}

}  // namespace media
