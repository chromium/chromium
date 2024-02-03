// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_MANAGER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_MANAGER_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "services/media_session/public/cpp/media_image.h"

namespace gfx {
class Size;
}  // namespace gfx

class GURL;

namespace media_session {

// MediaImageManager manages the MediaImage selection process.
//
// The scoring works as follows:
// - An image score is computed by multiplying the type score with the size
//   score.
// - The type score is between 0 and 1 and is based on the image MIME type
//   and/or file extension.
//     - PNG and JPEG are prefered than others.
//     - If unspecified, use the default type score (0.6).
// - The size score is between 0 and 1 and is computed by multiplying the
//   dominant size score and aspect ratio score:
//     - The dominant size score is between 0 and 1 and is computed using
//       |min_size| and |ideal_size|.
//         - If size < |min_size| (too small), the size score is 0.
//         - If |min_size| <= size <= |ideal_size|, the score increases
//           linearly from 0.2 to 1.
//         - If size > |ideal_size|, the score is |ideal_size| / size which
//           drops from 1 to 0.
//         - When size is "any", the size score is 0.8.
//         - If unspecified, use the default size score (0.4).
//     - The aspect ratio score is between 0 and 1 and is computed by dividing
//       the short edge length by the long edge.
class COMPONENT_EXPORT(MEDIA_SESSION_CPP) MediaImageManager {
 public:
  // Returns the image size score as a double between 0 and 1. The score will
  // be calculated using the size scoring algorithm described above.
  static double GetImageSizeScore(int min_size,
                                  int ideal_size,
                                  const gfx::Size& size);

  // The |min_size| is the min size of the images to select in px. The
  // |ideal_size| is the ideal size of the images to select in px.
  MediaImageManager(int min_size, int ideal_size);

  MediaImageManager(const MediaImageManager&) = delete;
  MediaImageManager& operator=(const MediaImageManager&) = delete;

  ~MediaImageManager();

  // Select the best image from the |images|. If an image could not be selected
  // then will return null.
  std::optional<MediaImage> SelectImage(const std::vector<MediaImage>& images);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaImageManagerTest,
                           CheckExpectedImageExtensionHashes);
  FRIEND_TEST_ALL_PREFIXES(MediaImageManagerTest, CheckExpectedImageTypeHashes);

  double GetImageScore(const MediaImage& image) const;

  static std::optional<double> GetImageExtensionScore(const GURL& url);

  static std::optional<double> GetImageTypeScore(const std::u16string& type);

  const int min_size_;
  const int ideal_size_;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_MANAGER_H_
