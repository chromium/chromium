// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_MEDIA_SESSION_CLIENT_H_
#define CONTENT_PUBLIC_TEST_TEST_MEDIA_SESSION_CLIENT_H_

#include <string>

#include "content/public/browser/media_session_client.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class TestMediaSessionClient : public content::MediaSessionClient {
 public:
  TestMediaSessionClient();
  ~TestMediaSessionClient() override;

  TestMediaSessionClient(const TestMediaSessionClient&) = delete;
  TestMediaSessionClient& operator=(const TestMediaSessionClient&) = delete;

  bool ShouldHideMetadata(
      content::BrowserContext* browser_context) const override;

  std::u16string GetTitlePlaceholder() const override;
  std::u16string GetSourceTitlePlaceholder() const override;
  std::u16string GetArtistPlaceholder() const override;
  std::u16string GetAlbumPlaceholder() const override;
  SkBitmap GetThumbnailPlaceholder() const override;

  void SetShouldHideMetadata(bool value);
  void SetTitlePlaceholder(std::u16string title);
  void SetSourceTitlePlaceholder(std::u16string source_title);
  void SetArtistPlaceholder(std::u16string artist);
  void SetAlbumPlaceholder(std::u16string album);
  void SetThumbnailPlaceholder(SkBitmap thumbnail);

 private:
  bool should_hide_metadata_ = false;
  std::u16string placeholder_title_;
  std::u16string placeholder_source_title_;
  std::u16string placeholder_artist_;
  std::u16string placeholder_album_;
  SkBitmap placeholder_thumbnail_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_MEDIA_SESSION_CLIENT_H_
