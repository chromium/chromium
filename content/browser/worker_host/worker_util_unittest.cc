// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_util.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(WorkerUtilTest, DoesCreatorAllowFileUrlSupport) {
  const url::Origin kHttpOrigin =
      url::Origin::Create(GURL("http://example.com"));
  const url::Origin kFileOrigin =
      url::Origin::Create(GURL("file:///path/to/page.html"));

  blink::web_pref::WebPreferences default_prefs;
  blink::web_pref::WebPreferences allow_file_prefs;
  allow_file_prefs.allow_file_access_from_file_urls = true;
  blink::web_pref::WebPreferences allow_universal_prefs;
  allow_universal_prefs.allow_universal_access_from_file_urls = true;

  // 1. Non-file origin should never allow file URL support.
  EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(kHttpOrigin, &default_prefs));
  EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(kHttpOrigin, &allow_file_prefs));
  EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(
      kHttpOrigin, /*web_preferences=*/nullptr,
      /*creator_worker_has_file_url_support=*/true));

  // 2. File origin without flags/preferences or parent support should be false.
  EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(kFileOrigin, &default_prefs));
  EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(
      kFileOrigin, /*web_preferences=*/nullptr,
      /*creator_worker_has_file_url_support=*/false));

  // 3. File origin with WebPreferences::allow_file_access_from_file_urls.
  EXPECT_TRUE(DoesCreatorAllowFileUrlSupport(kFileOrigin, &allow_file_prefs));

  // 4. File origin with WebPreferences::allow_universal_access_from_file_urls.
  EXPECT_TRUE(
      DoesCreatorAllowFileUrlSupport(kFileOrigin, &allow_universal_prefs));

  // 5. File origin inheriting from creator worker with file URL support.
  EXPECT_TRUE(DoesCreatorAllowFileUrlSupport(
      kFileOrigin, /*web_preferences=*/nullptr,
      /*creator_worker_has_file_url_support=*/true));

  // 6. File origin with switches::kAllowFileAccessFromFiles.
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kAllowFileAccessFromFiles);
    EXPECT_TRUE(DoesCreatorAllowFileUrlSupport(kFileOrigin, &default_prefs));
    EXPECT_TRUE(DoesCreatorAllowFileUrlSupport(
        kFileOrigin, /*web_preferences=*/nullptr,
        /*creator_worker_has_file_url_support=*/false));
    // Even with the switch, non-file origins should still be rejected.
    EXPECT_FALSE(DoesCreatorAllowFileUrlSupport(kHttpOrigin, &default_prefs));
  }
}

}  // namespace content
