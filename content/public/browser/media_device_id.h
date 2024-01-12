// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Media device IDs come in two flavors: The machine-wide unique ID of
// the device, which is what we use on the browser side, and one-way
// hashes over the unique ID and a security origin, which we provide
// to code on the renderer side as per-security-origin IDs.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_DEVICE_ID_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_DEVICE_ID_H_

#include <optional>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

namespace content {

// Generates a one-way hash of a device's unique ID usable by one
// particular security origin.
CONTENT_EXPORT std::string GetHMACForMediaDeviceID(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& raw_unique_id);

// Convenience method to check if |device_guid| is an HMAC of
// |raw_device_id| for |security_origin|.
CONTENT_EXPORT bool DoesMediaDeviceIDMatchHMAC(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& device_guid,
    const std::string& raw_unique_id);

// Returns the raw device ID for the given HMAC |hmac_device_id| for the given
// |security_origin| and |salt|. The result is passed via |callback| on the
// task runner where this function is called. If |hmac_device_id| is not a
// valid device ID nullopt is returned.
// The |callback| will be posted on the given |task_runner|.
CONTENT_EXPORT void GetMediaDeviceIDForHMAC(
    blink::mojom::MediaStreamType stream_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const std::optional<std::string>&)> callback);

CONTENT_EXPORT bool IsValidDeviceId(const std::string& device_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_DEVICE_ID_H_
