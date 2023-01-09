// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_browser.h"

#include <utility>

#include "build/build_config.h"
#include "content/public/common/user_agent.h"
#include "headless/public/version.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

using Options = headless::HeadlessBrowser::Options;
using Builder = headless::HeadlessBrowser::Options::Builder;

namespace headless {

namespace {
// Product name for building the default user agent string.
const char kHeadlessProductName[] = "HeadlessChrome";
constexpr gfx::Size kDefaultWindowSize(800, 600);

constexpr gfx::FontRenderParams::Hinting kDefaultFontRenderHinting =
    gfx::FontRenderParams::Hinting::HINTING_FULL;

}  // namespace

Options::Options(int argc, const char** argv)
    : argc(argc),
      argv(argv),
      gl_implementation(gl::kGLImplementationANGLEName),
      angle_implementation(gl::kANGLEImplementationSwiftShaderForWebGLName),
      user_agent(content::BuildUserAgentFromProduct(
          HeadlessBrowser::GetProductNameAndVersion())),
      window_size(kDefaultWindowSize),
      font_render_hinting(kDefaultFontRenderHinting) {}

Options::Options(Options&& options) = default;

Options::~Options() = default;

Options& Options::operator=(Options&& options) = default;

bool Options::DevtoolsServerEnabled() {
  return (devtools_pipe_enabled || !devtools_endpoint.IsEmpty());
}

Builder::Builder(int argc, const char** argv) : options_(argc, argv) {}

Builder::Builder() : options_(0, nullptr) {}

Builder::~Builder() = default;

Builder& Builder::SetUserAgent(const std::string& agent) {
  options_.user_agent = agent;
  return *this;
}

Builder& Builder::SetAcceptLanguage(const std::string& language) {
  options_.accept_language = language;
  return *this;
}

Builder& Builder::SetEnableBeginFrameControl(bool enable) {
  options_.enable_begin_frame_control = enable;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const net::HostPortPair& endpoint) {
  options_.devtools_endpoint = endpoint;
  return *this;
}

Builder& Builder::EnableDevToolsPipe() {
  options_.devtools_pipe_enabled = true;
  return *this;
}

Builder& Builder::SetProxyConfig(std::unique_ptr<net::ProxyConfig> config) {
  options_.proxy_config = std::move(config);
  return *this;
}

Builder& Builder::SetGLImplementation(const std::string& implementation) {
  options_.gl_implementation = implementation;
  return *this;
}

Builder& Builder::SetANGLEImplementation(const std::string& implementation) {
  options_.angle_implementation = implementation;
  return *this;
}

#if BUILDFLAG(IS_WIN)
Builder& Builder::SetInstance(HINSTANCE hinstance) {
  options_.instance = hinstance;
  return *this;
}

Builder& Builder::SetSandboxInfo(sandbox::SandboxInterfaceInfo* info) {
  options_.sandbox_info = info;
  return *this;
}
#endif  // BUILDFLAG(IS_WIN)

Builder& Builder::SetUserDataDir(const base::FilePath& dir) {
  options_.user_data_dir = dir;
  return *this;
}

Builder& Builder::SetWindowSize(const gfx::Size& size) {
  options_.window_size = size;
  return *this;
}

Builder& Builder::SetIncognitoMode(bool incognito) {
  options_.incognito_mode = incognito;
  return *this;
}

Builder& Builder::SetBlockNewWebContents(bool block) {
  options_.block_new_web_contents = block;
  return *this;
}

Builder& Builder::SetCrashReporterEnabled(bool enabled) {
  options_.enable_crash_reporter = enabled;
  return *this;
}

Builder& Builder::SetCrashDumpsDir(const base::FilePath& dir) {
  options_.crash_dumps_dir = dir;
  return *this;
}

Builder& Builder::SetFontRenderHinting(gfx::FontRenderParams::Hinting hinting) {
  options_.font_render_hinting = hinting;
  return *this;
}

Options Builder::Build() {
  return std::move(options_);
}

/// static
std::string HeadlessBrowser::GetProductNameAndVersion() {
  return std::string(kHeadlessProductName) + "/" + PRODUCT_VERSION;
}

}  // namespace headless
