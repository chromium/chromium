// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_

#include "gpu/config/gpu_info.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoEncodeAccelerator::Error,
                          media::VideoEncodeAccelerator::kErrorMax)

IPC_STRUCT_TRAITS_BEGIN(media::VideoDecodeAccelerator::Config)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(encryption_scheme)
  IPC_STRUCT_TRAITS_MEMBER(cdm_id)
  IPC_STRUCT_TRAITS_MEMBER(is_deferred_initialization_allowed)
  IPC_STRUCT_TRAITS_MEMBER(overlay_info)
  IPC_STRUCT_TRAITS_MEMBER(initial_expected_coded_size)
  IPC_STRUCT_TRAITS_MEMBER(supported_output_formats)
  IPC_STRUCT_TRAITS_MEMBER(sps)
  IPC_STRUCT_TRAITS_MEMBER(pps)
  IPC_STRUCT_TRAITS_MEMBER(container_color_space)
  IPC_STRUCT_TRAITS_MEMBER(target_color_space)
  IPC_STRUCT_TRAITS_MEMBER(hdr_metadata)
IPC_STRUCT_TRAITS_END()

#endif  // MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
