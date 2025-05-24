// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_REGISTERED_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_REGISTERED_FRAME_CONVERTER_H_

#include "base/memory/scoped_refptr.h"
#include "media/gpu/chromeos/frame_resource_converter.h"

namespace media {

class FrameRegistry;

// This class is used for converting FrameResources to opaque |VideoFrame|'s. It
// is constructed with a FrameRegistry which it uses to register frames that it
// converts. The output VideoFrame's |tracking_token| can be used as a handle to
// access the original frame in the FrameRegistry. The FrameRegistry retains a
// reference to the FrameResource passed to ConvertFrame() until the returned
// frame is destroyed.
class RegisteredFrameConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create(
      scoped_refptr<FrameRegistry> registry);

  // RegisteredFrameConverter is not moveable or copyable.
  RegisteredFrameConverter(const RegisteredFrameConverter&) = delete;
  RegisteredFrameConverter& operator=(const RegisteredFrameConverter&) = delete;

 private:
  explicit RegisteredFrameConverter(scoped_refptr<FrameRegistry> registry);
  ~RegisteredFrameConverter() override;

  // FrameConverter overrides
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;

  // A frame registry, indexed by the frame's tracking token. A reference to the
  // original FrameResource is taken until the generated VideoFrame is
  // destroyed.
  scoped_refptr<FrameRegistry> registry_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_REGISTERED_FRAME_CONVERTER_H_
