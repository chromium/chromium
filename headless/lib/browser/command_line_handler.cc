// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "headless/lib/browser/command_line_handler.h"

#include <cstdio>
#include <string_view>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "headless/public/switches.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

namespace {

void AppendSwitchMaybe(base::CommandLine& command_line,
                       std::string_view switch_constant) {
  if (!command_line.HasSwitch(switch_constant)) {
    command_line.AppendSwitch(switch_constant);
  }
}

void HandleDeterministicModeSwitch(base::CommandLine& command_line) {
  DCHECK(command_line.HasSwitch(switches::kDeterministicMode));

  AppendSwitchMaybe(command_line, switches::kEnableBeginFrameControl);

  // Compositor flags
  AppendSwitchMaybe(command_line,
                    ::switches::kRunAllCompositorStagesBeforeDraw);
  AppendSwitchMaybe(command_line,
                    ::switches::kDisableNewContentRenderingTimeout);
  // Ensure that image animations don't resync their animation timestamps when
  // looping back around.
  AppendSwitchMaybe(command_line,
                    blink::switches::kDisableImageAnimationResync);

  // Renderer flags
  AppendSwitchMaybe(command_line, ::switches::kDisableThreadedAnimation);
  AppendSwitchMaybe(command_line, ::switches::kDisableCheckerImaging);
}

bool HandleRemoteDebuggingPort(base::CommandLine& command_line,
                               HeadlessBrowser::Options& options) {
  DCHECK(command_line.HasSwitch(::switches::kRemoteDebuggingPort));

  int port;
  std::string port_str =
      command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
  if (!base::StringToInt(port_str, &port) ||
      !base::IsValueInRangeForNumericType<uint16_t>(port)) {
    LOG(ERROR) << "Invalid devtools server port: " << port_str;
    return false;
  }

  options.devtools_port = base::checked_cast<uint16_t>(port);
  return true;
}

void HandleProxyServer(base::CommandLine& command_line,
                       HeadlessBrowser::Options& options) {
  DCHECK(command_line.HasSwitch(switches::kProxyServer));

  std::string proxy_server =
      command_line.GetSwitchValueASCII(switches::kProxyServer);
  auto proxy_config = std::make_unique<net::ProxyConfig>();
  proxy_config->proxy_rules().ParseFromString(proxy_server);
  if (command_line.HasSwitch(switches::kProxyBypassList)) {
    std::string bypass_list =
        command_line.GetSwitchValueASCII(switches::kProxyBypassList);
    proxy_config->proxy_rules().bypass_rules.ParseFromString(bypass_list);
  }

  options.proxy_config = std::move(proxy_config);
}

bool HandleWindowSize(base::CommandLine& command_line,
                      HeadlessBrowser::Options& options) {
  DCHECK(command_line.HasSwitch(switches::kWindowSize));

  const std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kWindowSize);

  int width = 0;
  int height = 0;
  int n = sscanf(switch_value.c_str(), "%d%*[x,]%d", &width, &height);
  if (n != 2 || width < 0 || height < 0) {
    LOG(ERROR) << "Malformed window size: " << switch_value;
    return false;
  }

  options.window_size = gfx::Size(width, height);
  return true;
}

bool HandleScreenInfo(base::CommandLine& command_line,
                      HeadlessBrowser::Options& options) {
  DCHECK(command_line.HasSwitch(switches::kScreenInfo));

  const std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kScreenInfo);

  auto screen_info = HeadlessScreenInfo::FromString(switch_value);
  if (!screen_info.has_value()) {
    LOG(ERROR) << screen_info.error();
    return false;
  }

  options.screen_info_spec = switch_value;
  return true;
}

bool HandleFontRenderHinting(base::CommandLine& command_line,
                             HeadlessBrowser::Options& options) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kFontRenderHinting);

  gfx::FontRenderParams::Hinting font_render_hinting;
  static_assert(gfx::FontRenderParams::Hinting::HINTING_MAX == 3);
  if (switch_value == "full") {
    font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_FULL;
  } else if (switch_value == "medium") {
    font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_MEDIUM;
  } else if (switch_value == "slight") {
    font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_SLIGHT;
  } else if (switch_value == "none") {
    font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_NONE;
  } else {
    LOG(ERROR) << "Unknown font-render-hinting parameter value";
    return false;
  }

  options.font_render_hinting = font_render_hinting;
  return true;
}

base::FilePath EnsureDirectoryExists(const base::FilePath& file_path) {
  if (!base::DirectoryExists(file_path) && !base::CreateDirectory(file_path)) {
    PLOG(ERROR) << "Could not create directory " << file_path;
    return base::FilePath();
  }

  if (file_path.IsAbsolute()) {
    return file_path;
  }

  const base::FilePath absolute_file_path =
      base::MakeAbsoluteFilePath(file_path);
  if (absolute_file_path.empty()) {
    PLOG(ERROR) << "Invalid directory path " << file_path;
    return base::FilePath();
  }

  return absolute_file_path;
}

}  // namespace

bool HandleCommandLineSwitches(base::CommandLine& command_line,
                               HeadlessBrowser::Options& options) {
  if (command_line.HasSwitch(switches::kDeterministicMode)) {
    if (command_line.HasSwitch(::switches::kSitePerProcess)) {
      LOG(ERROR) << "Deterministic mode is not compatible with --"
                 << ::switches::kSitePerProcess << " switch.";
      return false;
    }
    HandleDeterministicModeSwitch(command_line);
  }

  if (command_line.HasSwitch(switches::kEnableBeginFrameControl)) {
    if (command_line.HasSwitch(::switches::kSitePerProcess)) {
      LOG(ERROR) << "Frame control is not compatible with --"
                 << ::switches::kSitePerProcess << " switch.";
      return false;
    }
    options.enable_begin_frame_control = true;
  }

  if (command_line.HasSwitch(::switches::kRemoteDebuggingPort)) {
    if (!HandleRemoteDebuggingPort(command_line, options)) {
      return false;
    }
  }
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPipe)) {
    options.devtools_pipe_enabled = true;
  }

  if (command_line.HasSwitch(switches::kProxyServer)) {
    HandleProxyServer(command_line, options);
  }

  if (command_line.HasSwitch(switches::kUserDataDir)) {
    const base::FilePath dir = EnsureDirectoryExists(
        command_line.GetSwitchValuePath(switches::kUserDataDir));
    if (dir.empty()) {
      return false;
    }
    options.user_data_dir = dir;

    if (!command_line.HasSwitch(switches::kIncognito)) {
      options.incognito_mode = false;
    }
  }

  if (command_line.HasSwitch(switches::kDiskCacheDir)) {
    const base::FilePath dir = EnsureDirectoryExists(
        command_line.GetSwitchValuePath(switches::kDiskCacheDir));
    if (dir.empty()) {
      return false;
    }
    options.disk_cache_dir = dir;
  }

  if (command_line.HasSwitch(switches::kWindowSize)) {
    if (!HandleWindowSize(command_line, options)) {
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kScreenInfo)) {
    if (!HandleScreenInfo(command_line, options)) {
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent)) {
      options.user_agent = user_agent;
    }
  }

  if (command_line.HasSwitch(switches::kAcceptLang)) {
    options.accept_language =
        command_line.GetSwitchValueASCII(switches::kAcceptLang);
  }

  if (command_line.HasSwitch(switches::kFontRenderHinting)) {
    if (!HandleFontRenderHinting(command_line, options)) {
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kBlockNewWebContents)) {
    options.block_new_web_contents = true;
  }

  if (command_line.HasSwitch(switches::kDisableLazyLoading)) {
    options.lazy_load_enabled = false;
  }

  if (command_line.HasSwitch(switches::kForceNewBrowsingInstance)) {
    options.force_new_browsing_instance = true;
  }

  return true;
}

}  // namespace headless
