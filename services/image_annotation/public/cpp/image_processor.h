// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_PUBLIC_CPP_IMAGE_PROCESSOR_H_
#define SERVICES_IMAGE_ANNOTATION_PUBLIC_CPP_IMAGE_PROCESSOR_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace image_annotation {

// Handles local (i.e. out-of-service) processing of an image on behalf of the
// image annotation service.
class ImageProcessor : public mojom::ImageProcessor {
 public:
  // We accept a "get pixels" callback so we don't have to store the pixel data
  // for (potentially huge) images.
  //
  // TODO(crbug.com/41432508): accept a more sophisticated interface to fetch
  //                         pixels; this will be required for iOS, where pixel
  //                         access entails a full image redownload.
  explicit ImageProcessor(base::RepeatingCallback<SkBitmap()> get_pixels);

  ImageProcessor(const ImageProcessor&) = delete;
  ImageProcessor& operator=(const ImageProcessor&) = delete;

  ~ImageProcessor() override;

  // Reencodes the image data for transmission to the service. Will be called by
  // the service if pixel data is needed.
  void GetJpgImageData(GetJpgImageDataCallback callback) override;

  // Returns a new pending remote to the Mojo interface for this image
  // processor.
  mojo::PendingRemote<mojom::ImageProcessor> GetPendingRemote();

 private:
  // TODO(crbug.com/41432508): tune these values.

  // The maximum number of pixels to transmit to the service. Images with too
  // many pixels will be scaled down prior to transmission.
  static constexpr int kMaxPixels = 512 * 512;

  // The quality parameter to use when encoding images before transmission.
  static constexpr int kJpgQuality = 65;

  const base::RepeatingCallback<SkBitmap()> get_pixels_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  mojo::ReceiverSet<mojom::ImageProcessor> receivers_;

  FRIEND_TEST_ALL_PREFIXES(ImageProcessorTest, ImageContent);
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_PUBLIC_CPP_IMAGE_PROCESSOR_H_
