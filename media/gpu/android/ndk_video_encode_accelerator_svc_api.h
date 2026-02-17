// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_SVC_API_H_
#define MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_SVC_API_H_

#include <media/NdkMediaCodec.h>

#include "base/no_destructor.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// TODO(crbug.com/469819307): These APIs are available in Android 17
// but are not yet exposed in the NDK headers used by Chromium. We declare them
// manually here to use them via dlsym. Once the NDK is updated to support these
// APIs, these forward declarations and typedefs should be removed in favor of
// including <media/NdkMediaCodec.h>.

// Forward declarations for opaque structs from new NDK APIs
struct AMediaCodecInfo;
struct ACodecEncoderCapabilities;

class MEDIA_GPU_EXPORT NdkVideoEncodeAcceleratorSvcApi {
 public:
  static const NdkVideoEncodeAcceleratorSvcApi* Get();

  // Function pointer typedefs
  using AMediaCodecStore_getCodecInfo_Type =
      media_status_t (*)(const char* name,
                         const AMediaCodecInfo** outCodecInfo);

  using AMediaCodecInfo_getEncoderCapabilities_Type =
      media_status_t (*)(const AMediaCodecInfo* info,
                         const ACodecEncoderCapabilities** outEncoderCaps);

  using ACodecEncoderCapabilities_getSupportedLayeringSchemas_Type =
      media_status_t (*)(const ACodecEncoderCapabilities* encoderCaps,
                         const char* const** outSupportedLayeringSchemaArrayPtr,
                         size_t* outCount);

  AMediaCodecStore_getCodecInfo_Type AMediaCodecStore_getCodecInfo = nullptr;
  AMediaCodecInfo_getEncoderCapabilities_Type
      AMediaCodecInfo_getEncoderCapabilities = nullptr;
  ACodecEncoderCapabilities_getSupportedLayeringSchemas_Type
      ACodecEncoderCapabilities_getSupportedLayeringSchemas = nullptr;

 private:
  friend class base::NoDestructor<NdkVideoEncodeAcceleratorSvcApi>;

  NdkVideoEncodeAcceleratorSvcApi();
  ~NdkVideoEncodeAcceleratorSvcApi() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_SVC_API_H_
