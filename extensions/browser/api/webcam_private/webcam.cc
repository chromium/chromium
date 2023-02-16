// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam.h"

namespace extensions {

Webcam::Webcam() = default;
Webcam::~Webcam() = default;

WebcamResource::WebcamResource(const std::string& owner_extension_id,
                               Webcam* webcam,
                               const std::string& webcam_id)
    : ApiResource(owner_extension_id), webcam_(webcam), webcam_id_(webcam_id) {
}

WebcamResource::~WebcamResource() {
}

Webcam* WebcamResource::GetWebcam() const {
  return webcam_.get();
}

const std::string WebcamResource::GetWebcamId() const {
  return webcam_id_;
}

bool WebcamResource::IsPersistent() const {
  return false;
}

}  // namespace extensions
