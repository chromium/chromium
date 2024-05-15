// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "crypto/hmac.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

namespace content {

using ::blink::mojom::MediaDeviceType;
using ::blink::mojom::MediaStreamType;

namespace {

void GotSalt(const std::string& frame_salt,
             const url::Origin& origin,
             bool has_focus,
             bool is_background,
             std::optional<ukm::SourceId> source_id,
             MediaDeviceSaltAndOriginCallback callback,
             bool are_persistent_device_ids_allowed,
             const std::string& salt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string device_id_salt = salt;
  if (!are_persistent_device_ids_allowed) {
    device_id_salt += frame_salt;
  }

  // |group_id_salt| must be unique per document, but it must also change if
  // cookies are cleared. Also, it must be different from |device_id_salt|,
  // thus appending a constant.
  std::string group_id_salt = base::StrCat({salt, frame_salt, "group_id"});

  MediaDeviceSaltAndOrigin salt_and_origin(std::move(device_id_salt), origin,
                                           std::move(group_id_salt), has_focus,
                                           is_background, std::move(source_id));
  std::move(callback).Run(salt_and_origin);
}

void FinalizeGetRawMediaDeviceIDForHMAC(
    MediaDeviceType type,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& source_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const MediaDeviceEnumeration& enumeration) {
  for (const auto& device : enumeration[static_cast<size_t>(type)]) {
    if (DoesRawMediaDeviceIDMatchHMAC(salt_and_origin, source_id,
                                      device.device_id)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), device.device_id));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::nullopt));
}

MediaDeviceType ConvertToMediaDeviceType(MediaStreamType stream_type) {
  switch (stream_type) {
    case MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return MediaDeviceType::kMediaAudioInput;
    case MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return MediaDeviceType::kMediaVideoInput;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return MediaDeviceType::kNumMediaDeviceTypes;
}

}  // namespace

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(
    std::string device_id_salt,
    url::Origin origin,
    std::string group_id_salt,
    bool has_focus,
    bool is_background,
    std::optional<ukm::SourceId> ukm_source_id)
    : device_id_salt_(std::move(device_id_salt)),
      group_id_salt_(std::move(group_id_salt)),
      origin_(std::move(origin)),
      ukm_source_id_(std::move(ukm_source_id)),
      has_focus_(has_focus),
      is_background_(is_background) {}

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(
    const MediaDeviceSaltAndOrigin& other) = default;
MediaDeviceSaltAndOrigin& MediaDeviceSaltAndOrigin::operator=(
    const MediaDeviceSaltAndOrigin& other) = default;

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(
    MediaDeviceSaltAndOrigin&& other) = default;
MediaDeviceSaltAndOrigin& MediaDeviceSaltAndOrigin::operator=(
    MediaDeviceSaltAndOrigin&& other) = default;

MediaDeviceSaltAndOrigin::~MediaDeviceSaltAndOrigin() = default;

MediaDeviceSaltAndOrigin MediaDeviceSaltAndOrigin::Empty() {
  return MediaDeviceSaltAndOrigin(/*device_id_salt=*/std::string(),
                                  /*origin=*/url::Origin());
}

void GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId render_frame_host_id,
                                 MediaDeviceSaltAndOriginCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!frame_host) {
    std::move(callback).Run(MediaDeviceSaltAndOrigin::Empty());
    return;
  }

  // Check that the frame is not in the prerendering state. Media playback is
  // deferred while prerendering. So this check should pass and ensures the
  // condition to call GetPageUkmSourceId below as the data collection policy
  // disallows recording UKMs while prerendering.
  CHECK(!frame_host->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  net::SiteForCookies site_for_cookies = frame_host->ComputeSiteForCookies();
  url::Origin origin = frame_host->GetLastCommittedOrigin();
  bool has_focus = frame_host->GetView() && frame_host->GetView()->HasFocus();
  std::optional<ukm::SourceId> source_id = frame_host->GetPageUkmSourceId();
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
  bool is_background =
      web_contents && web_contents->GetDelegate() &&
      web_contents->GetDelegate()->IsNeverComposited(web_contents);
  std::string frame_salt = frame_host->GetMediaDeviceIDSaltBase();

  GetContentClient()->browser()->GetMediaDeviceIDSalt(
      frame_host, site_for_cookies, frame_host->GetStorageKey(),
      base::BindOnce(&GotSalt, std::move(frame_salt), std::move(origin),
                     has_focus, is_background, std::move(source_id),
                     std::move(callback)));
}

blink::WebMediaDeviceInfo TranslateMediaDeviceInfo(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfo& device_info) {
  if (has_permission) {
    return blink::WebMediaDeviceInfo(
        GetHMACForRawMediaDeviceID(salt_and_origin, device_info.device_id),
        device_info.label,
        device_info.group_id.empty()
            ? std::string()
            : GetHMACForRawMediaDeviceID(salt_and_origin, device_info.group_id,
                                         /*use_group_salt=*/true),
        device_info.video_control_support, device_info.video_facing,
        device_info.availability);
  }
  return blink::WebMediaDeviceInfo(std::string(), std::string(), std::string(),
                                   media::VideoCaptureControlSupport(),
                                   blink::mojom::FacingMode::kNone);
}

blink::WebMediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  blink::WebMediaDeviceInfoArray result;
  for (const auto& device_info : device_infos) {
    result.push_back(
        TranslateMediaDeviceInfo(has_permission, salt_and_origin, device_info));
    if (!has_permission && result.back().device_id.empty()) {
      break;
    }
  }
  return result;
}

std::string CreateRandomMediaDeviceIDSalt() {
  return base::UnguessableToken::Create().ToString();
}

void GetHMACFromRawDeviceId(GlobalRenderFrameHostId render_frame_host_id,
                            const std::string& raw_device_id,
                            DeviceIdCallback hmac_device_id_callback) {
  auto got_salt = base::BindOnce(
      [](const std::string& raw_device_id,
         DeviceIdCallback hmac_device_id_callback,
         const MediaDeviceSaltAndOrigin& salt_and_origin) {
        std::move(hmac_device_id_callback)
            .Run(GetHMACForRawMediaDeviceID(salt_and_origin, raw_device_id));
      },
      raw_device_id, std::move(hmac_device_id_callback));
  GetMediaDeviceSaltAndOrigin(render_frame_host_id, std::move(got_salt));
}

void GetRawDeviceIdFromHMAC(GlobalRenderFrameHostId render_frame_host_id,
                            const std::string& hmac_device_id,
                            MediaDeviceType media_device_type,
                            OptionalDeviceIdCallback raw_device_id_callback) {
  auto got_salt = base::BindOnce(
      [](const std::string& hmac_device_id,
         OptionalDeviceIdCallback raw_device_id_callback,
         MediaDeviceType media_device_type,
         const MediaDeviceSaltAndOrigin& salt_and_origin) {
        // `GetRawDeviceIDForMediaDeviceHMAC()` needs to be called on the IO
        // thread since it performs a device enumeration.
        content::GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&GetRawDeviceIDForMediaDeviceHMAC, media_device_type,
                           salt_and_origin, hmac_device_id,
                           base::SequencedTaskRunner::GetCurrentDefault(),
                           std::move(raw_device_id_callback)));
      },
      hmac_device_id, std::move(raw_device_id_callback), media_device_type);

  GetMediaDeviceSaltAndOrigin(render_frame_host_id, std::move(got_salt));
}

std::string GetHMACForRawMediaDeviceID(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& raw_device_id,
    bool use_group_salt) {
  if (raw_device_id.empty() ||
      raw_device_id == media::AudioDeviceDescription::kDefaultDeviceId ||
      raw_device_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return raw_device_id;
  }

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8_t> digest(digest_length);
  bool result =
      hmac.Init(salt_and_origin.origin().Serialize()) &&
      hmac.Sign(
          raw_device_id + (use_group_salt ? salt_and_origin.group_id_salt()
                                          : salt_and_origin.device_id_salt()),
          &digest[0], digest.size());
  DCHECK(result);
  return base::ToLowerASCII(base::HexEncode(digest));
}

bool DoesRawMediaDeviceIDMatchHMAC(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& hmac_device_id,
    const std::string& raw_device_id) {
  return GetHMACForRawMediaDeviceID(salt_and_origin, raw_device_id) ==
         hmac_device_id;
}

void GetRawDeviceIDForMediaStreamHMAC(
    MediaStreamType stream_type,
    MediaDeviceSaltAndOrigin salt_and_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OptionalDeviceIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stream_type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         stream_type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  MediaDeviceType device_type = ConvertToMediaDeviceType(stream_type);
  GetRawDeviceIDForMediaDeviceHMAC(device_type, std::move(salt_and_origin),
                                   std::move(hmac_device_id),
                                   std::move(task_runner), std::move(callback));
}

void GetRawDeviceIDForMediaDeviceHMAC(
    MediaDeviceType device_type,
    MediaDeviceSaltAndOrigin salt_and_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OptionalDeviceIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes requested_types;
  requested_types[static_cast<size_t>(device_type)] = true;
  MediaStreamManager::GetInstance()->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::BindOnce(&FinalizeGetRawMediaDeviceIDForHMAC, device_type,
                     std::move(salt_and_origin), std::move(hmac_device_id),
                     std::move(task_runner), std::move(callback)));
}

}  // namespace content
