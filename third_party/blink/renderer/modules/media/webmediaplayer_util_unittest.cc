// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

TEST(GetMediaURLScheme, MissingUnknown) {
  EXPECT_EQ(media::mojom::MediaURLScheme::kMissing,
            GetMediaURLScheme(WebURL()));
  EXPECT_EQ(media::mojom::MediaURLScheme::kUnknown,
            GetMediaURLScheme(KURL("abcd://ab")));
}

TEST(GetMediaURLScheme, WebCommon) {
  EXPECT_EQ(media::mojom::MediaURLScheme::kFtp,
            GetMediaURLScheme(KURL("ftp://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kHttp,
            GetMediaURLScheme(KURL("http://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kHttps,
            GetMediaURLScheme(KURL("https://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kData,
            GetMediaURLScheme(KURL("data://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kBlob,
            GetMediaURLScheme(KURL("blob://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kJavascript,
            GetMediaURLScheme(KURL("javascript://abc.123")));
}

TEST(GetMediaURLScheme, Files) {
  EXPECT_EQ(media::mojom::MediaURLScheme::kFile,
            GetMediaURLScheme(KURL("file://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kFileSystem,
            GetMediaURLScheme(KURL("filesystem:file://abc/123")));
}

TEST(GetMediaURLScheme, Android) {
  EXPECT_EQ(media::mojom::MediaURLScheme::kContent,
            GetMediaURLScheme(KURL("content://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kContentId,
            GetMediaURLScheme(KURL("cid://abc.123")));
}

TEST(GetMediaURLScheme, Chrome) {
  EXPECT_EQ(media::mojom::MediaURLScheme::kChrome,
            GetMediaURLScheme(KURL("chrome://abc.123")));
  EXPECT_EQ(media::mojom::MediaURLScheme::kChromeExtension,
            GetMediaURLScheme(KURL("chrome-extension://abc.123")));
}

}  // namespace blink
