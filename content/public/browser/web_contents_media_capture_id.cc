// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/public/browser/web_contents_media_capture_id.h"

#include <string_view>
#include <tuple>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace {
constexpr char kDisableLocalEchoFlag[] = "local_echo=false";
constexpr char kOptionStart = '?';
constexpr char kOptionSeparator = '&';

bool ExtractTabCaptureTarget(const std::string& device_id_param,
                             int* render_process_id,
                             int* main_render_frame_id) {
  const std::string device_scheme = content::kWebContentsCaptureScheme;
  if (!base::StartsWith(device_id_param, device_scheme,
                        base::CompareCase::SENSITIVE))
    return false;

  const std::string device_id = device_id_param.substr(device_scheme.size());

  const size_t sep_pos = device_id.find(':');
  if (sep_pos == std::string::npos)
    return false;

  const std::string_view component1(device_id.data(), sep_pos);
  size_t end_pos = device_id.find('?');
  if (end_pos == std::string::npos)
    end_pos = device_id.length();
  const std::string_view component2(device_id.data() + sep_pos + 1,
                                    end_pos - sep_pos - 1);

  return (base::StringToInt(component1, render_process_id) &&
          base::StringToInt(component2, main_render_frame_id));
}

bool ExtractOptions(const std::string& device_id,
                    bool* disable_local_echo) {
  DCHECK(disable_local_echo);
  *disable_local_echo = false;

  // Find the option part of the string and just do a naive string compare since
  // there are no other options in the |device_id| to account for (at the time
  // of this writing).
  size_t option_pos = device_id.find(kOptionStart);
  if (option_pos == std::string::npos)
    return true;

  size_t option_pos_end;
  while (option_pos < device_id.length()) {
    option_pos_end = device_id.find(kOptionSeparator, option_pos + 1);
    if (option_pos_end == std::string::npos)
      option_pos_end = device_id.length();
    const std::string_view component(device_id.data() + option_pos + 1,
                                     option_pos_end - option_pos - 1);

    if (component.compare(kDisableLocalEchoFlag) == 0)
      *disable_local_echo = true;
    else  // Some unknown parameter is specified, and thus this ID is invalid.
      return false;

    option_pos = option_pos_end;
  }
  return true;
}

}  // namespace

namespace content {

const char kWebContentsCaptureScheme[] = "web-contents-media-stream://";

bool WebContentsMediaCaptureId::operator<(
    const WebContentsMediaCaptureId& other) const {
  return std::tie(render_process_id, main_render_frame_id, disable_local_echo) <
         std::tie(other.render_process_id, other.main_render_frame_id,
                  other.disable_local_echo);
}

bool WebContentsMediaCaptureId::operator==(
    const WebContentsMediaCaptureId& other) const {
  return std::tie(render_process_id, main_render_frame_id,
                  disable_local_echo) == std::tie(other.render_process_id,
                                                  other.main_render_frame_id,
                                                  other.disable_local_echo);
}

bool WebContentsMediaCaptureId::is_null() const {
  return (render_process_id < 0) || (main_render_frame_id < 0);
}

std::string WebContentsMediaCaptureId::ToString() const {
  std::string s = kWebContentsCaptureScheme;
  s.append(base::NumberToString(render_process_id));
  s.append(":");
  s.append(base::NumberToString(main_render_frame_id));

  if (disable_local_echo) {
    s += kOptionStart;
    s.append(kDisableLocalEchoFlag);
  }

  return s;
}

// static
bool WebContentsMediaCaptureId::Parse(const std::string& str,
                                      WebContentsMediaCaptureId* output_id) {
  int render_process_id;
  int main_render_frame_id;
  if (!ExtractTabCaptureTarget(str, &render_process_id, &main_render_frame_id))
    return false;

  bool disable_local_echo;
  if (!ExtractOptions(str, &disable_local_echo))
    return false;

  if (output_id) {
    output_id->render_process_id = render_process_id;
    output_id->main_render_frame_id = main_render_frame_id;
    output_id->disable_local_echo = disable_local_echo;
  }

  return true;
}


}  // namespace content
