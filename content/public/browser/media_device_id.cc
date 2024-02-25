// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_device_id.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/media/media_devices_util.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "third_party/blink/public/common/mediastream/media_device_id.h"

namespace content {

std::string GetHMACForMediaDeviceID(const std::string& salt,
                                    const url::Origin& security_origin,
                                    const std::string& raw_unique_id) {
  return GetHMACForRawMediaDeviceID(
      MediaDeviceSaltAndOrigin(salt, security_origin), raw_unique_id);
}

bool DoesMediaDeviceIDMatchHMAC(const std::string& salt,
                                const url::Origin& security_origin,
                                const std::string& device_guid,
                                const std::string& raw_unique_id) {
  return DoesRawMediaDeviceIDMatchHMAC(
      MediaDeviceSaltAndOrigin(salt, security_origin), device_guid,
      raw_unique_id);
}

void GetMediaDeviceIDForHMAC(
    blink::mojom::MediaStreamType stream_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  MediaDeviceSaltAndOrigin salt_and_origin(salt, security_origin);
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetRawDeviceIDForMediaStreamHMAC(
        stream_type, std::move(salt_and_origin), std::move(hmac_device_id),
        std::move(task_runner), std::move(callback));
  } else {
    GetIOThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GetRawDeviceIDForMediaStreamHMAC, stream_type,
                       std::move(salt_and_origin), std::move(hmac_device_id),
                       std::move(task_runner), std::move(callback)));
  }
}

bool IsValidDeviceId(const std::string& device_id) {
  return blink::IsValidMediaDeviceId(device_id);
}

}  // namespace content
