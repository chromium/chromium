// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/cpp/video_encoder.h"

#include <stddef.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_VideoEncoder_0_1>() {
  return PPB_VIDEOENCODER_INTERFACE_0_1;
}

template <>
const char* interface_name<PPB_VideoEncoder_0_2>() {
  return PPB_VIDEOENCODER_INTERFACE_0_2;
}

// This struct is used to adapt
// CompletionCallbackWithOutput<std::vector<PP_VideoProfileDescription>>
// to the pre-0.2 APIs, which return PP_VideoProfileDescription_0_1.
// This struct is allocated on the heap, and deleted in
// CallbackProfileDescriptionConverter.
struct CallbackProfileDescription_0_1 {
  explicit CallbackProfileDescription_0_1(const CompletionCallbackWithOutput<
      std::vector<PP_VideoProfileDescription> >& cc)
      : output_profiles(&profiles),
        original_output_profiles(cc.output()),
        original_callback(cc.pp_completion_callback()) {}
  std::vector<PP_VideoProfileDescription_0_1> profiles;
  ArrayOutputAdapter<PP_VideoProfileDescription_0_1> output_profiles;
  PP_ArrayOutput original_output_profiles;
  PP_CompletionCallback original_callback;
};

// Converts data from a 0.1 style callback to 0.2 callback.
void CallbackProfileDescriptionConverter(void* user_data, int32_t result) {
  CallbackProfileDescription_0_1* data =
      static_cast<CallbackProfileDescription_0_1*>(user_data);
  if (result >= 0) {
    PP_VideoProfileDescription* original_profiles =
        static_cast<PP_VideoProfileDescription*>(
            data->original_output_profiles.GetDataBuffer(
                data->original_output_profiles.user_data,
                static_cast<uint32_t>(data->profiles.size()),
                static_cast<uint32_t>(sizeof(PP_VideoProfileDescription))));

    for (size_t i = 0; i < data->profiles.size(); i++) {
      const PP_VideoProfileDescription_0_1& profile = data->profiles[i];

      original_profiles[i].profile = profile.profile;
      original_profiles[i].max_resolution = profile.max_resolution;
      original_profiles[i].max_framerate_numerator =
          profile.max_framerate_numerator;
      original_profiles[i].max_framerate_denominator =
          profile.max_framerate_denominator;
      original_profiles[i].hardware_accelerated =
          PP_FromBool(profile.acceleration == PP_HARDWAREACCELERATION_ONLY);
    }
  }

  // Now execute the original callback.
  PP_RunCompletionCallback(&data->original_callback, result);
  delete data;
}

}  // namespace

VideoEncoder::VideoEncoder() {}

VideoEncoder::VideoEncoder(const InstanceHandle& instance) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoEncoder_0_2>()->Create(instance.pp_instance()));
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoEncoder_0_1>()->Create(instance.pp_instance()));
  }
}

VideoEncoder::VideoEncoder(const VideoEncoder& other) : Resource(other) {}

VideoEncoder& VideoEncoder::operator=(const VideoEncoder& other) {
  Resource::operator=(other);
  return *this;
}

int32_t VideoEncoder::GetSupportedProfiles(const CompletionCallbackWithOutput<
    std::vector<PP_VideoProfileDescription> >& cc) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->GetSupportedProfiles(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    // Data for our callback wrapper. The callback handler will delete it.
    CallbackProfileDescription_0_1* data =
        new CallbackProfileDescription_0_1(cc);
    return get_interface<PPB_VideoEncoder_0_1>()->GetSupportedProfiles(
        pp_resource(), data->output_profiles.pp_array_output(),
        PP_MakeCompletionCallback(&CallbackProfileDescriptionConverter, data));
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::Initialize(const PP_VideoFrame_Format& input_format,
                                 const Size& input_visible_size,
                                 const PP_VideoProfile& output_profile,
                                 const uint32_t initial_bitrate,
                                 PP_HardwareAcceleration acceleration,
                                 const CompletionCallback& cc) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->Initialize(
        pp_resource(), input_format, &input_visible_size.pp_size(),
        output_profile, initial_bitrate, acceleration,
        cc.pp_completion_callback());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->Initialize(
        pp_resource(), input_format, &input_visible_size.pp_size(),
        output_profile, initial_bitrate, acceleration,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::GetFramesRequired() {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->GetFramesRequired(
        pp_resource());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetFramesRequired(
        pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VideoEncoder::GetFrameCodedSize(Size* coded_size) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->GetFrameCodedSize(
        pp_resource(), &coded_size->pp_size());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetFrameCodedSize(
        pp_resource(), &coded_size->pp_size());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VideoEncoder::GetVideoFrame(
    const CompletionCallbackWithOutput<VideoFrame>& cc) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->GetVideoFrame(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetVideoFrame(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::Encode(const VideoFrame& video_frame,
                             bool force_keyframe,
                             const CompletionCallback& cc) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->Encode(
        pp_resource(), video_frame.pp_resource(), PP_FromBool(force_keyframe),
        cc.pp_completion_callback());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->Encode(
        pp_resource(), video_frame.pp_resource(), PP_FromBool(force_keyframe),
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::GetBitstreamBuffer(
    const CompletionCallbackWithOutput<PP_BitstreamBuffer>& cc) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    return get_interface<PPB_VideoEncoder_0_2>()->GetBitstreamBuffer(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetBitstreamBuffer(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void VideoEncoder::RecycleBitstreamBuffer(
    const PP_BitstreamBuffer& bitstream_buffer) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    get_interface<PPB_VideoEncoder_0_2>()->RecycleBitstreamBuffer(
        pp_resource(), &bitstream_buffer);
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->RecycleBitstreamBuffer(
        pp_resource(), &bitstream_buffer);
  }
}

void VideoEncoder::RequestEncodingParametersChange(uint32_t bitrate,
                                                   uint32_t framerate) {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    get_interface<PPB_VideoEncoder_0_2>()->RequestEncodingParametersChange(
        pp_resource(), bitrate, framerate);
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->RequestEncodingParametersChange(
        pp_resource(), bitrate, framerate);
  }
}

void VideoEncoder::Close() {
  if (has_interface<PPB_VideoEncoder_0_2>()) {
    get_interface<PPB_VideoEncoder_0_2>()->Close(pp_resource());
  } else if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->Close(pp_resource());
  }
}

}  // namespace pp
