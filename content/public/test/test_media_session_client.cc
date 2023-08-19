// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_media_session_client.h"

namespace content {

TestMediaSessionClient::TestMediaSessionClient() = default;

TestMediaSessionClient::~TestMediaSessionClient() = default;

bool TestMediaSessionClient::ShouldHideMetadata(
    content::BrowserContext* browser_context) const {
  return should_hide_metadata_;
}

std::u16string TestMediaSessionClient::GetTitlePlaceholder() const {
  return placeholder_title_;
}

std::u16string TestMediaSessionClient::GetArtistPlaceholder() const {
  return placeholder_artist_;
}

std::u16string TestMediaSessionClient::GetAlbumPlaceholder() const {
  return placeholder_album_;
}

SkBitmap TestMediaSessionClient::GetThumbnailPlaceholder() const {
  return placeholder_thumbnail_;
}

void TestMediaSessionClient::SetShouldHideMetadata(bool value) {
  should_hide_metadata_ = value;
}

void TestMediaSessionClient::SetTitlePlaceholder(std::u16string title) {
  placeholder_title_ = title;
}

void TestMediaSessionClient::SetArtistPlaceholder(std::u16string artist) {
  placeholder_artist_ = artist;
}

void TestMediaSessionClient::SetAlbumPlaceholder(std::u16string album) {
  placeholder_album_ = album;
}

void TestMediaSessionClient::SetThumbnailPlaceholder(SkBitmap thumbnail) {
  placeholder_thumbnail_ = thumbnail;
}

}  // namespace content
