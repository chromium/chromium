// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_options.h"

#include <string>
#include <utility>

namespace headless {

namespace {

template <class T>
const T& ReturnOverriddenValue(const std::optional<T>& value,
                               const T& default_value) {
  return value ? *value : default_value;
}

}  // namespace

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::~HeadlessBrowserContextOptions() = default;

HeadlessBrowserContextOptions& HeadlessBrowserContextOptions::operator=(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowser::Options* options)
    : browser_options_(options) {}

const std::string& HeadlessBrowserContextOptions::accept_language() const {
  return ReturnOverriddenValue(accept_language_,
                               browser_options_->accept_language);
}

const std::string& HeadlessBrowserContextOptions::user_agent() const {
  return ReturnOverriddenValue(user_agent_, browser_options_->user_agent);
}

const net::ProxyConfig* HeadlessBrowserContextOptions::proxy_config() const {
  if (proxy_config_)
    return proxy_config_.get();
  return browser_options_->proxy_config.get();
}

const gfx::Size& HeadlessBrowserContextOptions::window_size() const {
  return ReturnOverriddenValue(window_size_, browser_options_->window_size);
}

const base::FilePath& HeadlessBrowserContextOptions::user_data_dir() const {
  return ReturnOverriddenValue(user_data_dir_, browser_options_->user_data_dir);
}

const base::FilePath& HeadlessBrowserContextOptions::disk_cache_dir() const {
  return ReturnOverriddenValue(disk_cache_dir_,
                               browser_options_->disk_cache_dir);
}

bool HeadlessBrowserContextOptions::incognito_mode() const {
  return ReturnOverriddenValue(incognito_mode_,
                               browser_options_->incognito_mode);
}

bool HeadlessBrowserContextOptions::block_new_web_contents() const {
  return ReturnOverriddenValue(block_new_web_contents_,
                               browser_options_->block_new_web_contents);
}

gfx::FontRenderParams::Hinting
HeadlessBrowserContextOptions::font_render_hinting() const {
  return ReturnOverriddenValue(font_render_hinting_,
                               browser_options_->font_render_hinting);
}

}  // namespace headless
