// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"

GaiaUrlsOverriderForTesting::GaiaUrlsOverriderForTesting(
    base::CommandLine* command_line,
    const std::string& url_key,
    const std::string& url_value)
    : command_line_(command_line) {
  // Initialize `GaiaConfig` from the updated `switches::kGaiaConfigContents`.
  command_line_->AppendSwitchASCII(
      switches::kGaiaConfigContents,
      base::ReplaceStringPlaceholders("{\"urls\": {\"$1\": {\"url\": \"$2\"}}}",
                                      {url_key, url_value}, nullptr));
  GaiaConfig::ResetInstanceForTesting();

  // Replace `GaiaUrls` singleton with the temporary scoped instance.
  test_gaia_urls_ = std::make_unique<GaiaUrls>();
  GaiaUrls::SetInstanceForTesting(test_gaia_urls_.get());
}

GaiaUrlsOverriderForTesting::~GaiaUrlsOverriderForTesting() {
  GaiaUrls::SetInstanceForTesting(nullptr);

  // Force `GaiaConfig` to re-initialize from command line switches.
  command_line_->RemoveSwitch(switches::kGaiaConfigContents);
  GaiaConfig::ResetInstanceForTesting();
}
