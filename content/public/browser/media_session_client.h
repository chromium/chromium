// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_CLIENT_H_

#include <string>

#include "content/common/content_export.h"

namespace content {
class BrowserContext;
}

class SkBitmap;

namespace content {

// Interface for a client to participate in media session logic. The primary use
// for this API is for embedders to set whether media information should be
// hidden from OS’ media controls. Additionally, this API will be used for
// embedders to set their own media metadata placeholders for display when the
// media information is hidden.
class CONTENT_EXPORT MediaSessionClient {
 public:
  MediaSessionClient(const MediaSessionClient&) = delete;
  MediaSessionClient& operator=(const MediaSessionClient&) = delete;

  MediaSessionClient();
  virtual ~MediaSessionClient();

  // Return the Media Session client.
  static MediaSessionClient* Get();

  virtual bool ShouldHideMetadata(BrowserContext* browser_context) const = 0;

  virtual std::u16string GetTitlePlaceholder() const = 0;
  virtual std::u16string GetSourceTitlePlaceholder() const = 0;
  virtual std::u16string GetArtistPlaceholder() const = 0;
  virtual std::u16string GetAlbumPlaceholder() const = 0;
  virtual SkBitmap GetThumbnailPlaceholder() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_CLIENT_H_
