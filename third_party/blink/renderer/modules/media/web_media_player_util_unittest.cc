// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/platform/media/media_player_util.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

TEST(GetMediaURLScheme, MissingUnknown) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kMissing,
            GetMediaURLScheme(WebURL()));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kUnknown,
            GetMediaURLScheme(KURL("abcd://ab")));
}

TEST(GetMediaURLScheme, WebCommon) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kFtp,
            GetMediaURLScheme(KURL("ftp://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kHttp,
            GetMediaURLScheme(KURL("http://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kHttps,
            GetMediaURLScheme(KURL("https://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kData,
            GetMediaURLScheme(KURL("data://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kBlob,
            GetMediaURLScheme(KURL("blob://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kJavascript,
            GetMediaURLScheme(KURL("javascript://abc.test")));
}

TEST(GetMediaURLScheme, Files) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kFile,
            GetMediaURLScheme(KURL("file://abc.test")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kFileSystem,
            GetMediaURLScheme(KURL("filesystem:file://abc/123")));
}

TEST(GetMediaURLScheme, Android) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kContent,
            GetMediaURLScheme(KURL("content://abc.123")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kContentId,
            GetMediaURLScheme(KURL("cid://abc.123")));
}

TEST(GetMediaURLScheme, Chrome) {
  test::TaskEnvironment task_environment;
  SchemeRegistry::RegisterURLSchemeAsWebUIForTest("chrome");
  CommonSchemeRegistry::RegisterURLSchemeAsExtension("chrome-extension");
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kChrome,
            GetMediaURLScheme(KURL("chrome://abc.123")));
  EXPECT_EQ(media::mojom::blink::MediaURLScheme::kChromeExtension,
            GetMediaURLScheme(KURL("chrome-extension://abc.123")));
  CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest("chrome-extension");
  SchemeRegistry::RemoveURLSchemeAsWebUIForTest("chrome");
}

}  // namespace blink
