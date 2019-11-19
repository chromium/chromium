// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SYSTEM_INFO_H_
#define SERVICES_AUDIO_SYSTEM_INFO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_system_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/traced_service_ref.h"

namespace media {
class AudioManager;
}

namespace audio {

class SystemInfo : public mojom::SystemInfo {
 public:
  explicit SystemInfo(media::AudioManager* audio_manager);
  ~SystemInfo() override;

  void Bind(mojo::PendingReceiver<mojom::SystemInfo> receiver,
            TracedServiceRef context_ref);

 private:
  // audio::mojom::SystemInfo implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      GetInputStreamParametersCallback callback) override;
  void GetOutputStreamParameters(
      const std::string& device_id,
      GetOutputStreamParametersCallback callback) override;
  void HasInputDevices(HasInputDevicesCallback callback) override;
  void HasOutputDevices(HasOutputDevicesCallback callback) override;
  void GetInputDeviceDescriptions(
      GetInputDeviceDescriptionsCallback callback) override;
  void GetOutputDeviceDescriptions(
      GetOutputDeviceDescriptionsCallback callback) override;
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      GetAssociatedOutputDeviceIDCallback callback) override;
  void GetInputDeviceInfo(const std::string& input_device_id,
                          GetInputDeviceInfoCallback callback) override;

  media::AudioSystemHelper helper_;

  // Each receiver increases ref count of the service context, so that the
  // service knows when it is in use.
  mojo::ReceiverSet<mojom::SystemInfo, TracedServiceRef> receivers_;

  // Validates thread-safe access to |bindings_| only. |helper_| takes care of
  // its thread safety/affinity itself.
  SEQUENCE_CHECKER(binding_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SystemInfo);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SYSTEM_INFO_H_
