// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/command_line_handler.h"

#include <cstdio>

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "headless/public/switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

namespace {

// By default listen to incoming DevTools connections on localhost.
const char kLocalHost[] = "localhost";

void HandleDeterministicModeSwitch(base::CommandLine& command_line) {
  DCHECK(command_line.HasSwitch(switches::kDeterministicMode));

  command_line.AppendSwitch(switches::kEnableBeginFrameControl);

  // Compositor flags
  command_line.AppendSwitch(::switches::kRunAllCompositorStagesBeforeDraw);
  command_line.AppendSwitch(::switches::kDisableNewContentRenderingTimeout);
  // Ensure that image animations don't resync their animation timestamps when
  // looping back around.
  command_line.AppendSwitch(blink::switches::kDisableImageAnimationResync);

  // Renderer flags
  command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
  command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);
}

bool HandleRemoteDebuggingPort(base::CommandLine& command_line,
                               HeadlessBrowser::Options::Builder& builder) {
  DCHECK(command_line.HasSwitch(::switches::kRemoteDebuggingPort));

  net::IPAddress address;
  std::string address_str = kLocalHost;
  if (command_line.HasSwitch(switches::kRemoteDebuggingAddress)) {
    address_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingAddress);
    if (!address.AssignFromIPLiteral(address_str)) {
      LOG(ERROR) << "Invalid devtools server address: " << address_str;
      return false;
    }
  }
  int port;
  std::string port_str =
      command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
  if (!base::StringToInt(port_str, &port) ||
      !base::IsValueInRangeForNumericType<uint16_t>(port)) {
    LOG(ERROR) << "Invalid devtools server port: " << port_str;
    return false;
  }
  const net::HostPortPair endpoint(address_str,
                                   base::checked_cast<uint16_t>(port));
  builder.EnableDevToolsServer(endpoint);
  return true;
}

void HandleProxyServer(base::CommandLine& command_line,
                       HeadlessBrowser::Options::Builder& builder) {
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
  builder.SetProxyConfig(std::move(proxy_config));
}

bool HandleWindowSize(base::CommandLine& command_line,
                      HeadlessBrowser::Options::Builder& builder) {
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

  builder.SetWindowSize(gfx::Size(width, height));
  return true;
}

bool HandleFontRenderHinting(base::CommandLine& command_line,
                             HeadlessBrowser::Options::Builder& builder) {
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

  builder.SetFontRenderHinting(font_render_hinting);
  return true;
}

}  // namespace

bool HandleCommandLineSwitches(base::CommandLine& command_line,
                               HeadlessBrowser::Options::Builder& builder) {
  if (command_line.HasSwitch(switches::kDeterministicMode)) {
    HandleDeterministicModeSwitch(command_line);
  }

  if (command_line.HasSwitch(switches::kEnableBeginFrameControl)) {
    builder.SetEnableBeginFrameControl(true);
  }

  if (command_line.HasSwitch(::switches::kRemoteDebuggingPort)) {
    if (!HandleRemoteDebuggingPort(command_line, builder)) {
      return false;
    }
  }
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPipe)) {
    builder.EnableDevToolsPipe();
  }

  if (command_line.HasSwitch(switches::kProxyServer)) {
    HandleProxyServer(command_line, builder);
  }

  if (command_line.HasSwitch(switches::kUserDataDir)) {
    builder.SetUserDataDir(
        command_line.GetSwitchValuePath(switches::kUserDataDir));
    if (!command_line.HasSwitch(switches::kIncognito)) {
      builder.SetIncognitoMode(false);
    }
  }

  if (command_line.HasSwitch(switches::kWindowSize)) {
    if (!HandleWindowSize(command_line, builder)) {
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent)) {
      builder.SetUserAgent(user_agent);
    }
  }

  if (command_line.HasSwitch(switches::kAcceptLang)) {
    builder.SetAcceptLanguage(
        command_line.GetSwitchValueASCII(switches::kAcceptLang));
  }

  if (command_line.HasSwitch(switches::kFontRenderHinting)) {
    if (!HandleFontRenderHinting(command_line, builder)) {
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kBlockNewWebContents)) {
    builder.SetBlockNewWebContents(true);
  }

  if (command_line.HasSwitch(switches::kDisableLazyLoading)) {
    builder.SetEnableLazyLoading(false);
  }

  return true;
}

}  // namespace headless
