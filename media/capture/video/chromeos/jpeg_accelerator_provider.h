// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_JPEG_ACCELERATOR_PROVIDER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_JPEG_ACCELERATOR_PROVIDER_H_

#include "base/threading/thread.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "components/chromeos_camera/common/jpeg_encode_accelerator.mojom.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/jpeg_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

using MojoJpegEncodeAcceleratorFactoryCB = base::RepeatingCallback<void(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>)>;
using MojoMjpegDecodeAcceleratorFactoryCB = base::RepeatingCallback<void(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>)>;

class CAPTURE_EXPORT JpegAcceleratorProviderImpl final
    : public cros::mojom::JpegAcceleratorProvider,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  JpegAcceleratorProviderImpl(MojoMjpegDecodeAcceleratorFactoryCB jda_factory,
                              MojoJpegEncodeAcceleratorFactoryCB jea_factory);
  ~JpegAcceleratorProviderImpl() override;
  JpegAcceleratorProviderImpl(const JpegAcceleratorProviderImpl&) = delete;
  JpegAcceleratorProviderImpl& operator=(const JpegAcceleratorProviderImpl&) =
      delete;

  void GetJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override;
  void GetMjpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override;

 private:
  void AddReceiver(mojo::ScopedMessagePipeHandle message_pipe);

  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  MojoMjpegDecodeAcceleratorFactoryCB jda_factory_;

  MojoJpegEncodeAcceleratorFactoryCB jea_factory_;

  mojo::ReceiverSet<cros::mojom::JpegAcceleratorProvider> receiver_set_;

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};

  base::WeakPtrFactory<JpegAcceleratorProviderImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_JPEG_ACCELERATOR_PROVIDER_H_
