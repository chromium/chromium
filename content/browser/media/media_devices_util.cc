// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

namespace {

std::string GetDefaultMediaDeviceIDOnUIThread(
    blink::MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame_host)
    return std::string();

  RenderFrameHostDelegate* delegate = frame_host->delegate();
  if (!delegate)
    return std::string();

  blink::mojom::MediaStreamType media_stream_type;
  switch (device_type) {
    case blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      media_stream_type = blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
      break;
    case blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      media_stream_type = blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
      break;
    default:
      return std::string();
  }

  return delegate->GetDefaultMediaDeviceID(media_stream_type);
}

// This function is intended for testing purposes. It returns an empty string
// if no default device is supplied via the command line.
std::string GetDefaultMediaDeviceIDFromCommandLine(
    blink::MediaDeviceType device_type) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeDeviceForMediaStream);
  // Optional comma delimited parameters to the command line can specify values
  // for the default device IDs.
  // Examples: "video-input-default-id=mycam, audio-input-default-id=mymic"
  base::StringTokenizer option_tokenizer(option, ", ");
  option_tokenizer.set_quote_chars("\"");

  while (option_tokenizer.GetNext()) {
    std::vector<std::string> param =
        base::SplitString(option_tokenizer.token(), "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (param.size() != 2u) {
      DLOG(WARNING) << "Forgot a value '" << option << "'? Use name=value for "
                    << switches::kUseFakeDeviceForMediaStream << ".";
      return std::string();
    }

    if (device_type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT &&
        param.front() == "audio-input-default-id") {
      return param.back();
    } else if (device_type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT &&
               param.front() == "video-input-default-id") {
      return param.back();
    }
  }

  return std::string();
}

}  // namespace

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin() = default;

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(std::string device_id_salt,
                                                   std::string group_id_salt,
                                                   url::Origin origin)
    : device_id_salt(std::move(device_id_salt)),
      group_id_salt(std::move(group_id_salt)),
      origin(std::move(origin)) {}

void GetDefaultMediaDeviceID(
    blink::MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(const std::string&)>& callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    std::string command_line_default_device_id =
        GetDefaultMediaDeviceIDFromCommandLine(device_type);
    if (!command_line_default_device_id.empty()) {
      callback.Run(command_line_default_device_id);
      return;
    }
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&GetDefaultMediaDeviceIDOnUIThread, device_type,
                 render_process_id, render_frame_id),
      callback);
}

MediaDeviceSaltAndOrigin GetMediaDeviceSaltAndOrigin(int render_process_id,
                                                     int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_id);

  url::Origin origin;
  GURL url;
  GURL site_for_cookies;
  url::Origin top_level_origin;
  std::string frame_salt;

  if (frame_host) {
    origin = frame_host->GetLastCommittedOrigin();
    url = frame_host->GetLastCommittedURL();
    site_for_cookies = frame_host->ComputeSiteForCookies();
    top_level_origin = frame_host->frame_tree_node()
                           ->frame_tree()
                           ->GetMainFrame()
                           ->GetLastCommittedOrigin();
    frame_salt = frame_host->GetMediaDeviceIDSaltBase();
  }

  bool are_persistent_ids_allowed = false;
  std::string device_id_salt;
  std::string group_id_salt;
  if (process_host) {
    are_persistent_ids_allowed =
        GetContentClient()->browser()->ArePersistentMediaDeviceIDsAllowed(
            process_host->GetBrowserContext(), url, site_for_cookies,
            top_level_origin);
    device_id_salt = process_host->GetBrowserContext()->GetMediaDeviceIDSalt();
    group_id_salt = device_id_salt;
  }

  // If persistent IDs are not allowed, append |frame_salt| to make it
  // specific to the current document.
  if (!are_persistent_ids_allowed)
    device_id_salt += frame_salt;

  // |group_id_salt| must be unique per document, but it must also change if
  // cookies are cleared. Also, it must be different from |device_id_salt|,
  // thus appending a constant.
  group_id_salt += frame_salt + "groupid";

  return {std::move(device_id_salt), std::move(group_id_salt),
          std::move(origin)};
}

blink::WebMediaDeviceInfo TranslateMediaDeviceInfo(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfo& device_info) {
  return blink::WebMediaDeviceInfo(
      GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                              salt_and_origin.origin, device_info.device_id),
      has_permission ? device_info.label : std::string(),
      device_info.group_id.empty()
          ? std::string()
          : GetHMACForMediaDeviceID(salt_and_origin.group_id_salt,
                                    salt_and_origin.origin,
                                    device_info.group_id),
      has_permission ? device_info.video_facing
                     : media::MEDIA_VIDEO_FACING_NONE);
}

blink::WebMediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  blink::WebMediaDeviceInfoArray result;
  for (const auto& device_info : device_infos) {
    result.push_back(
        TranslateMediaDeviceInfo(has_permission, salt_and_origin, device_info));
  }
  return result;
}

}  // namespace content
