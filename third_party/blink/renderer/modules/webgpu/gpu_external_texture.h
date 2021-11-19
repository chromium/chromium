// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class GPUExternalTextureDescriptor;
class WebGPUMailboxTexture;

class GPUExternalTexture : public DawnObject<WGPUExternalTexture> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUExternalTexture* Create(
      GPUDevice* device,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  explicit GPUExternalTexture(
      GPUDevice* device,
      WGPUExternalTexture externalTexture,
      scoped_refptr<WebGPUMailboxTexture> mailbox_texture);

  GPUExternalTexture(const GPUExternalTexture&) = delete;
  GPUExternalTexture& operator=(const GPUExternalTexture&) = delete;

  void Destroy();

 private:
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
