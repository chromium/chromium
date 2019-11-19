// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_PHOTO_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_PHOTO_CAPABILITIES_H_

#include "media/capture/mojom/image_capture.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/imagecapture/media_settings_range.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PhotoCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PhotoCapabilities() = default;
  ~PhotoCapabilities() override = default;

  MediaSettingsRange* imageHeight() const { return image_height_; }
  void SetImageHeight(MediaSettingsRange* value) { image_height_ = value; }

  MediaSettingsRange* imageWidth() const { return image_width_; }
  void SetImageWidth(MediaSettingsRange* value) { image_width_ = value; }

  Vector<String> fillLightMode() const;
  void SetFillLightMode(Vector<media::mojom::blink::FillLightMode> modes) {
    fill_light_modes_ = modes;
  }

  String redEyeReduction() const;
  void SetRedEyeReduction(
      media::mojom::blink::RedEyeReduction red_eye_reduction) {
    red_eye_reduction_ = red_eye_reduction;
  }
  bool IsRedEyeReductionControllable() const;

  void Trace(blink::Visitor*) override;

 private:
  Member<MediaSettingsRange> image_height_;
  Member<MediaSettingsRange> image_width_;
  Vector<media::mojom::blink::FillLightMode> fill_light_modes_;
  media::mojom::blink::RedEyeReduction red_eye_reduction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_PHOTO_CAPABILITIES_H_
