// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace headless {

// Represents options which can be customized for a given BrowserContext.
// Provides a fallback to default browser-side options when an option
// is not set for a particular BrowserContext.
class HeadlessBrowserContextOptions {
 public:
  HeadlessBrowserContextOptions(HeadlessBrowserContextOptions&& options);

  HeadlessBrowserContextOptions(const HeadlessBrowserContextOptions&) = delete;
  HeadlessBrowserContextOptions& operator=(
      const HeadlessBrowserContextOptions&) = delete;

  ~HeadlessBrowserContextOptions();

  HeadlessBrowserContextOptions& operator=(
      HeadlessBrowserContextOptions&& options);

  const std::string& product_name_and_version() const;
  const std::string& accept_language() const;
  const std::string& user_agent() const;

  // See HeadlessBrowser::Options::proxy_config.
  const net::ProxyConfig* proxy_config() const;

  const gfx::Size& window_size() const;

  // See HeadlessBrowser::Options::user_data_dir.
  const base::FilePath& user_data_dir() const;

  // See HeadlessBrowser::Options::incognito_mode.
  bool incognito_mode() const;

  // See HeadlessBrowser::Options::block_new_web_contents.
  bool block_new_web_contents() const;

  // See HeadlessBrowser::Options::font_render_hinting.
  gfx::FontRenderParams::Hinting font_render_hinting() const;

  // Callback that is invoked to override WebPreferences for RenderViews
  // created within this HeadlessBrowserContext.
  base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
  override_web_preferences_callback() const;

 private:
  friend class HeadlessBrowserContext::Builder;

  explicit HeadlessBrowserContextOptions(HeadlessBrowser::Options*);

  raw_ptr<HeadlessBrowser::Options> browser_options_;

  absl::optional<std::string> product_name_and_version_;
  absl::optional<std::string> accept_language_;
  absl::optional<std::string> user_agent_;
  std::unique_ptr<net::ProxyConfig> proxy_config_;
  absl::optional<std::string> host_resolver_rules_;
  absl::optional<gfx::Size> window_size_;
  absl::optional<base::FilePath> user_data_dir_;
  absl::optional<bool> incognito_mode_;
  absl::optional<bool> block_new_web_contents_;
  absl::optional<
      base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>>
      override_web_preferences_callback_;

  absl::optional<gfx::FontRenderParams::Hinting> font_render_hinting_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
