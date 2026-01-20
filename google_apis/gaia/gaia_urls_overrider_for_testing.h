// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_URLS_OVERRIDER_FOR_TESTING_H_
#define GOOGLE_APIS_GAIA_GAIA_URLS_OVERRIDER_FOR_TESTING_H_

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"

class GaiaUrls;

// Helper class that overrides a specific url in `GaiaConfig` and creates a
// scoped version of `GaiaUrls` with the new value. Useful in tests when
// `GaiaConfig` needs to be updated with the `net::EmbeddedTestServer`'s base
// url.
class GaiaUrlsOverriderForTesting {
 public:
  GaiaUrlsOverriderForTesting(const std::string& url_key,
                              const std::string& url_value);
  GaiaUrlsOverriderForTesting(const GaiaUrlsOverriderForTesting&) = delete;
  GaiaUrlsOverriderForTesting& operator=(const GaiaUrlsOverriderForTesting&) =
      delete;
  ~GaiaUrlsOverriderForTesting();

 private:
  // Closure for restoring the original `GaiaConfig`.
  base::ScopedClosureRunner scoped_config_override_;

  // Scoped version of `GaiaUrls` that used instead of the original
  // singleton.
  std::unique_ptr<GaiaUrls> test_gaia_urls_;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_URLS_OVERRIDER_FOR_TESTING_H_
