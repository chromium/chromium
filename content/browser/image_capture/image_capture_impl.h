// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_
#define CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_service.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class ImageCaptureImpl final
    : public content::DocumentService<media::mojom::ImageCapture> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::ImageCapture> receiver);

  ImageCaptureImpl(const ImageCaptureImpl&) = delete;
  ImageCaptureImpl& operator=(const ImageCaptureImpl&) = delete;

  void GetPhotoState(const std::string& source_id,
                     GetPhotoStateCallback callback) override;

  void SetPhotoOptions(const std::string& source_id,
                       media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

  void TakePhoto(const std::string& source_id,
                 TakePhotoCallback callback) override;

 private:
  ImageCaptureImpl(RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<media::mojom::ImageCapture> receiver);
  ~ImageCaptureImpl() override;

  void OnGetPhotoState(GetPhotoStateCallback callback,
                       media::mojom::PhotoStatePtr);

  bool HasPanTiltZoomPermissionGranted();

  base::WeakPtrFactory<ImageCaptureImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_
