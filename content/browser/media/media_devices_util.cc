// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

namespace {

std::string GetDefaultMediaDeviceIDOnUIThread(MediaDeviceType device_type,
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
    case MediaDeviceType::MEDIA_AUDIO_INPUT:
      media_stream_type = blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
      break;
    case MediaDeviceType::MEDIA_VIDEO_INPUT:
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
    MediaDeviceType device_type) {
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
    std::vector<base::StringPiece> param = base::SplitStringPiece(
        option_tokenizer.token_piece(), "=", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    if (param.size() != 2u) {
      DLOG(WARNING) << "Forgot a value '" << option << "'? Use name=value for "
                    << switches::kUseFakeDeviceForMediaStream << ".";
      return std::string();
    }

    if (device_type == MediaDeviceType::MEDIA_AUDIO_INPUT &&
        param.front() == "audio-input-default-id") {
      return std::string(param.back());
    } else if (device_type == MediaDeviceType::MEDIA_VIDEO_INPUT &&
               param.front() == "video-input-default-id") {
      return std::string(param.back());
    }
  }

  return std::string();
}

}  // namespace

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin() = default;

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(std::string device_id_salt,
                                                   std::string group_id_salt,
                                                   url::Origin origin,
                                                   bool has_focus,
                                                   bool is_background)
    : device_id_salt(std::move(device_id_salt)),
      group_id_salt(std::move(group_id_salt)),
      origin(std::move(origin)),
      has_focus(has_focus),
      is_background(is_background) {}

MediaDeviceSaltAndOrigin::MediaDeviceSaltAndOrigin(
    const MediaDeviceSaltAndOrigin& other) = default;

void GetDefaultMediaDeviceID(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(const std::string&)> callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    std::string command_line_default_device_id =
        GetDefaultMediaDeviceIDFromCommandLine(device_type);
    if (!command_line_default_device_id.empty()) {
      std::move(callback).Run(command_line_default_device_id);
      return;
    }
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetDefaultMediaDeviceIDOnUIThread, device_type,
                     render_process_id, render_frame_id),
      std::move(callback));
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
  net::SiteForCookies site_for_cookies;
  url::Origin top_level_origin;
  std::string frame_salt;
  bool has_focus = true;
  bool is_background = false;

  if (frame_host) {
    origin = frame_host->GetLastCommittedOrigin();
    url = frame_host->GetLastCommittedURL();
    site_for_cookies = frame_host->ComputeSiteForCookies();
    top_level_origin = frame_host->frame_tree_node()
                           ->frame_tree()
                           .GetMainFrame()
                           ->GetLastCommittedOrigin();
    frame_salt = frame_host->GetMediaDeviceIDSaltBase();
    has_focus = frame_host->GetView() && frame_host->GetView()->HasFocus();

    auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
    is_background =
        web_contents && web_contents->GetDelegate() &&
        web_contents->GetDelegate()->IsNeverComposited(web_contents);
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
          std::move(origin), has_focus, is_background};
}

blink::WebMediaDeviceInfo TranslateMediaDeviceInfo(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfo& device_info) {
  return blink::WebMediaDeviceInfo(
      !base::FeatureList::IsEnabled(features::kEnumerateDevicesHideDeviceIDs) ||
              has_permission
          ? GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                                    salt_and_origin.origin,
                                    device_info.device_id)
          : std::string(),
      has_permission ? device_info.label : std::string(),
      device_info.group_id.empty()
          ? std::string()
          : GetHMACForMediaDeviceID(salt_and_origin.group_id_salt,
                                    salt_and_origin.origin,
                                    device_info.group_id),
      has_permission ? device_info.video_control_support
                     : media::VideoCaptureControlSupport(),
      has_permission ? device_info.video_facing
                     : blink::mojom::FacingMode::NONE);
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
