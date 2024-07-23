// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/video_encoder_resource.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/thunk.h"

using ppapi::proxy::ResourceMessageTestSink;

namespace ppapi {
namespace proxy {

namespace {

class MockCompletionCallback {
 public:
  MockCompletionCallback()
      : called_(false),
        call_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  bool called() { return called_; }
  int32_t result() { return result_; }

  void WaitUntilCalled() { call_event_.Wait(); }

  void Reset() {
    called_ = false;
    call_event_.Reset();
  }

  static void Callback(void* user_data, int32_t result) {
    MockCompletionCallback* that =
        reinterpret_cast<MockCompletionCallback*>(user_data);
    that->call_event_.Signal();
    that->called_ = true;
    that->result_ = result;
  }

 private:
  bool called_;
  int32_t result_;
  base::WaitableEvent call_event_;
};

class VideoEncoderResourceTest : public PluginProxyTest,
                                 public MediaStreamBufferManager::Delegate {
 public:
  VideoEncoderResourceTest()
      : encoder_iface_(thunk::GetPPB_VideoEncoder_0_2_Thunk()),
        encoder_iface_0_1_(thunk::GetPPB_VideoEncoder_0_1_Thunk()),
        video_frames_manager_(this) {}
  ~VideoEncoderResourceTest() override {}

  const PPB_VideoEncoder_0_2* encoder_iface() const { return encoder_iface_; }
  const PPB_VideoEncoder_0_1* encoder_iface_0_1() const {
    return encoder_iface_0_1_;
  }

  const uint32_t kBitstreamBufferSize = 4000;
  const uint32_t kBitstreamBufferCount = 5;
  const uint32_t kVideoFrameCount = 3;
  const uint32_t kBitrate = 200000;

  const PP_Size kFrameSize = PP_MakeSize(640, 480);

  void SendReply(const ResourceMessageCallParams& params,
                 int32_t result,
                 const IPC::Message& nested_message) {
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    PluginMessageFilter::DispatchResourceReplyForTest(reply_params,
                                                      nested_message);
  }

  void SendReplyWithHandle(const ResourceMessageCallParams& params,
                           int32_t result,
                           const IPC::Message& nested_message,
                           SerializedHandle handle) {
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    reply_params.AppendHandle(std::move(handle));
    PluginMessageFilter::DispatchResourceReplyForTest(reply_params,
                                                      nested_message);
  }

  void SendReplyWithHandles(const ResourceMessageCallParams& params,
                            int32_t result,
                            const IPC::Message& nested_message,
                            std::vector<SerializedHandle> handles) {
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    for (auto& handle : handles)
      reply_params.AppendHandle(std::move(handle));
    PluginMessageFilter::DispatchResourceReplyForTest(reply_params,
                                                      nested_message);
  }

  PP_Resource CreateEncoder() {
    PP_Resource result = encoder_iface()->Create(pp_instance());
    return result;
  }

  void CreateBitstreamSharedMemory(uint32_t buffer_size, uint32_t nb_buffers) {
    shared_memory_bitstreams_.clear();
    for (uint32_t i = 0; i < nb_buffers; ++i) {
      base::UnsafeSharedMemoryRegion region =
          base::UnsafeSharedMemoryRegion::Create(buffer_size);
      ASSERT_TRUE(region.IsValid());
      shared_memory_bitstreams_.push_back(std::move(region));
    }
  }

  void CreateVideoFramesSharedMemory(uint32_t frame_length,
                                     uint32_t frame_count) {
    uint32_t buffer_length =
        frame_length + sizeof(ppapi::MediaStreamBuffer::Video);
    base::UnsafeSharedMemoryRegion shared_memory_frames =
        base::UnsafeSharedMemoryRegion::Create(buffer_length * frame_count);
    ASSERT_TRUE(shared_memory_frames.IsValid());
    ASSERT_TRUE(video_frames_manager_.SetBuffers(
        frame_count, buffer_length, std::move(shared_memory_frames), true));
    for (int32_t i = 0; i < video_frames_manager_.number_of_buffers(); ++i) {
      ppapi::MediaStreamBuffer::Video* buffer =
        &(video_frames_manager_.GetBufferPointer(i)->video);
      buffer->header.size = buffer_length;
      buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
      buffer->format = PP_VIDEOFRAME_FORMAT_I420;
      buffer->size = kFrameSize;
      buffer->data_size = frame_length;
    }
  }

  PP_Resource CreateAndInitializeEncoder() {
    PP_Resource encoder = CreateEncoder();
    PP_Size size = kFrameSize;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder, PP_VIDEOFRAME_FORMAT_I420, &size, PP_VIDEOPROFILE_H264MAIN,
        kBitrate, PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    if (result != PP_OK_COMPLETIONPENDING)
      return 0;
    ResourceMessageCallParams params;
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_Initialize::ID, &params, &msg))
      return 0;
    sink().ClearMessages();

    SendInitializeReply(params, PP_OK, kVideoFrameCount, kFrameSize);
    CreateBitstreamSharedMemory(kBitstreamBufferSize, kBitstreamBufferCount);
    SendBitstreamBuffers(params, kBitstreamBufferSize);

    if (!cb.called() || cb.result() != PP_OK)
      return 0;

    return encoder;
  }

  int32_t CallGetFramesRequired(PP_Resource pp_encoder) {
    return encoder_iface()->GetFramesRequired(pp_encoder);
  }

  int32_t CallGetFrameCodedSize(PP_Resource pp_encoder, PP_Size* coded_size) {
    return encoder_iface()->GetFrameCodedSize(pp_encoder, coded_size);
  }

  int32_t CallGetVideoFrame(PP_Resource pp_encoder,
                            PP_Resource* video_frame,
                            MockCompletionCallback* cb) {
    return encoder_iface()->GetVideoFrame(
        pp_encoder, video_frame, PP_MakeOptionalCompletionCallback(
                                     &MockCompletionCallback::Callback, cb));
  }

  int32_t CallFirstGetVideoFrame(PP_Resource pp_encoder,
                                 PP_Resource* video_frame,
                                 MockCompletionCallback* cb) {
    int32_t result = encoder_iface()->GetVideoFrame(
        pp_encoder, video_frame, PP_MakeOptionalCompletionCallback(
                                     &MockCompletionCallback::Callback, cb));
    if (result != PP_OK_COMPLETIONPENDING)
      return result;

    ResourceMessageCallParams params;
    CheckGetVideoFramesMsg(&params);

    uint32_t frame_length = kFrameSize.width * kFrameSize.height * 2;
    CreateVideoFramesSharedMemory(frame_length, kVideoFrameCount);
    SendGetVideoFramesReply(params, kVideoFrameCount, frame_length, kFrameSize);

    return result;
  }

  int32_t CallEncode(PP_Resource pp_encoder,
                     PP_Resource video_frame,
                     PP_Bool force_keyframe,
                     MockCompletionCallback* cb) {
    return encoder_iface()->Encode(pp_encoder, video_frame, force_keyframe,
                                   PP_MakeOptionalCompletionCallback(
                                       &MockCompletionCallback::Callback, cb));
  }

  int32_t CallCompleteEncode(PP_Resource pp_encoder,
                             PP_Resource video_frame,
                             PP_Bool force_keyframe,
                             MockCompletionCallback* cb) {
    int32_t result =
        encoder_iface()->Encode(pp_encoder, video_frame, force_keyframe,
                                PP_MakeOptionalCompletionCallback(
                                    &MockCompletionCallback::Callback, cb));
    if (result != PP_OK_COMPLETIONPENDING)
      return result;

    ResourceMessageCallParams params;
    uint32_t frame_id;
    bool forced_keyframe;
    if (!CheckEncodeMsg(&params, &frame_id, &forced_keyframe))
      return PP_ERROR_FAILED;

    SendEncodeReply(params, frame_id);

    return result;
  }

  int32_t CallGetBitstreamBuffer(PP_Resource pp_encoder,
                                 PP_BitstreamBuffer* bitstream_buffer,
                                 MockCompletionCallback* cb) {
    return encoder_iface()->GetBitstreamBuffer(
        pp_encoder, bitstream_buffer,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          cb));
  }

  void CallRecycleBitstreamBuffer(PP_Resource pp_encoder,
                                  const PP_BitstreamBuffer& buffer) {
    encoder_iface()->RecycleBitstreamBuffer(pp_encoder, &buffer);
  }

  void CallRequestEncodingParametersChange(PP_Resource pp_encoder,
                                           uint32_t bitrate,
                                           uint32_t framerate) {
    encoder_iface()->RequestEncodingParametersChange(pp_encoder, bitrate,
                                                     framerate);
  }

  void CallClose(PP_Resource pp_encoder) {
    encoder_iface()->Close(pp_encoder);
  }

  void SendGetSupportedProfilesReply(
      const ResourceMessageCallParams& params,
      const std::vector<PP_VideoProfileDescription>& profiles) {
    SendReply(params, PP_OK,
              PpapiPluginMsg_VideoEncoder_GetSupportedProfilesReply(profiles));
  }

  void SendInitializeReply(const ResourceMessageCallParams& params,
                           int32_t success,
                           uint32_t input_frame_count,
                           const PP_Size& input_coded_size) {
    SendReply(params, success, PpapiPluginMsg_VideoEncoder_InitializeReply(
                                   input_frame_count, input_coded_size));
  }

  void SendBitstreamBuffers(const ResourceMessageCallParams& params,
                            uint32_t buffer_length) {
    std::vector<SerializedHandle> handles;
    for (const auto& mem : shared_memory_bitstreams_) {
      ASSERT_EQ(mem.GetSize(), buffer_length);
      base::UnsafeSharedMemoryRegion region = mem.Duplicate();
      ASSERT_TRUE(region.IsValid());
      handles.push_back(SerializedHandle(std::move(region)));
    }
    SendReplyWithHandles(
        params, PP_OK,
        PpapiPluginMsg_VideoEncoder_BitstreamBuffers(buffer_length),
        std::move(handles));
  }

  void SendGetVideoFramesReply(const ResourceMessageCallParams& params,
                               uint32_t frame_count,
                               uint32_t frame_length,
                               const PP_Size& size) {
    base::UnsafeSharedMemoryRegion region =
        video_frames_manager_.region().Duplicate();
    ASSERT_TRUE(region.IsValid());
    SendReplyWithHandle(
        params, PP_OK,
        PpapiPluginMsg_VideoEncoder_GetVideoFramesReply(
            frame_count, frame_length + sizeof(MediaStreamBuffer::Video), size),
        SerializedHandle(std::move(region)));
  }

  void SendEncodeReply(const ResourceMessageCallParams& params,
                       uint32_t frame_id) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoEncoder_EncodeReply(frame_id));
  }

  void SendBitstreamBufferReady(const ResourceMessageCallParams& params,
                                uint32_t buffer_id,
                                uint32_t buffer_size,
                                bool keyframe) {
    SendReply(params, PP_OK,
              PpapiPluginMsg_VideoEncoder_BitstreamBufferReady(
                  buffer_id, buffer_size, PP_FromBool(keyframe)));
  }

  void SendNotifyError(const ResourceMessageCallParams& params, int32_t error) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoEncoder_NotifyError(error));
  }

  bool CheckGetSupportedProfilesMsg(ResourceMessageCallParams* params) {
    IPC::Message msg;
    return sink().GetFirstResourceCallMatching(
        PpapiHostMsg_VideoEncoder_GetSupportedProfiles::ID, params, &msg);
  }

  bool CheckInitializeMsg(ResourceMessageCallParams* params,
                          PP_VideoFrame_Format* input_format,
                          struct PP_Size* input_visible_size,
                          PP_VideoProfile* output_profile,
                          uint32_t* bitrate,
                          PP_HardwareAcceleration* acceleration) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_Initialize::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<PpapiHostMsg_VideoEncoder_Initialize>(
        msg, input_format, input_visible_size, output_profile, bitrate,
        acceleration);
  }

  bool CheckGetVideoFramesMsg(ResourceMessageCallParams* params) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_GetVideoFrames::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return true;
  }

  bool CheckEncodeMsg(ResourceMessageCallParams* params,
                      uint32_t* frame_id,
                      bool* keyframe) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_Encode::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<PpapiHostMsg_VideoEncoder_Encode>(msg, frame_id,
                                                           keyframe);
  }

  bool CheckRecycleBitstreamBufferMsg(ResourceMessageCallParams* params,
                                      uint32_t* buffer_id) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer>(
        msg, buffer_id);
  }

  bool CheckRequestEncodingParametersChangeMsg(
      ResourceMessageCallParams* params,
      uint32_t* bitrate,
      uint32_t* framerate) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange::ID,
            params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<
        PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange>(msg, bitrate,
                                                                   framerate);
  }

  bool CheckIsVideoFrame(PP_Resource video_frame) {
    return thunk::GetPPB_VideoFrame_0_1_Thunk()->IsVideoFrame(video_frame);
  }

  bool CheckIsVideoFrameValid(PP_Resource video_frame) {
    PP_Size frame_size;
    return thunk::GetPPB_VideoFrame_0_1_Thunk()->GetSize(
               video_frame, &frame_size) == PP_TRUE;
  }

 private:
  // MediaStreamBufferManager::Delegate:
  void OnNewBufferEnqueued() override {}

  const PPB_VideoEncoder_0_2* encoder_iface_;
  const PPB_VideoEncoder_0_1* encoder_iface_0_1_;

  std::vector<base::UnsafeSharedMemoryRegion> shared_memory_bitstreams_;

  MediaStreamBufferManager video_frames_manager_;
};

void* ForwardUserData(void* user_data,
                      uint32_t element_count,
                      uint32_t element_size) {
  return user_data;
}

}  // namespace

TEST_F(VideoEncoderResourceTest, GetSupportedProfiles) {
  // Verifies that GetSupportedProfiles calls into the renderer and
  // the we get the right results back.
  {
    LockingResourceReleaser encoder(CreateEncoder());
    PP_VideoProfileDescription profiles[2];
    PP_ArrayOutput output;
    output.user_data = &profiles[0];
    output.GetDataBuffer = ForwardUserData;
    ResourceMessageCallParams params;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->GetSupportedProfiles(
        encoder.get(), output, PP_MakeOptionalCompletionCallback(
                                   &MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    ASSERT_TRUE(CheckGetSupportedProfilesMsg(&params));

    std::vector<PP_VideoProfileDescription> profiles_response;
    PP_VideoProfileDescription profile;
    profile.profile = PP_VIDEOPROFILE_H264MAIN;
    profile.max_resolution.width = 1920;
    profile.max_resolution.height = 1080;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;
    profile.hardware_accelerated = PP_TRUE;
    profiles_response.push_back(profile);
    profile.profile = PP_VIDEOPROFILE_VP8_ANY;
    profile.max_resolution.width = 1920;
    profile.max_resolution.height = 1080;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;
    profile.hardware_accelerated = PP_FALSE;
    profiles_response.push_back(profile);

    SendGetSupportedProfilesReply(params, profiles_response);
    ASSERT_EQ(profiles_response.size(), static_cast<uint32_t>(cb.result()));
    ASSERT_EQ(0, memcmp(&profiles[0], &profiles_response[0], sizeof(profiles)));
  }
}

TEST_F(VideoEncoderResourceTest, GetSupportedProfiles0_1) {
  // Verifies that GetSupportedProfiles calls into the renderer and
  // the we get the right results back.
  {
    LockingResourceReleaser encoder(CreateEncoder());
    PP_VideoProfileDescription_0_1 profiles[2];
    PP_ArrayOutput output;
    output.user_data = &profiles[0];
    output.GetDataBuffer = ForwardUserData;
    ResourceMessageCallParams params;
    MockCompletionCallback cb;
    int32_t result = encoder_iface_0_1()->GetSupportedProfiles(
        encoder.get(), output, PP_MakeOptionalCompletionCallback(
                                   &MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    ASSERT_TRUE(CheckGetSupportedProfilesMsg(&params));

    std::vector<PP_VideoProfileDescription> profiles_response;
    PP_VideoProfileDescription profile;
    profile.profile = PP_VIDEOPROFILE_H264MAIN;
    profile.max_resolution.width = 1920;
    profile.max_resolution.height = 1080;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;
    profile.hardware_accelerated = PP_TRUE;
    profiles_response.push_back(profile);
    profile.profile = PP_VIDEOPROFILE_VP8_ANY;
    profile.max_resolution.width = 1920;
    profile.max_resolution.height = 1080;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;
    profile.hardware_accelerated = PP_FALSE;
    profiles_response.push_back(profile);

    SendGetSupportedProfilesReply(params, profiles_response);

    ASSERT_EQ(profiles_response.size(), static_cast<uint32_t>(cb.result()));

    for (uint32_t i = 0; i < profiles_response.size(); i++) {
      ASSERT_EQ(profiles_response[i].profile, profiles[i].profile);
      ASSERT_EQ(profiles_response[i].max_resolution.width,
                profiles[i].max_resolution.width);
      ASSERT_EQ(profiles_response[i].max_resolution.height,
                profiles[i].max_resolution.height);
      ASSERT_EQ(profiles_response[i].max_framerate_numerator,
                profiles[i].max_framerate_numerator);
      ASSERT_EQ(profiles_response[i].max_framerate_denominator,
                profiles[i].max_framerate_denominator);
      if (profiles_response[i].hardware_accelerated)
        ASSERT_EQ(PP_HARDWAREACCELERATION_ONLY, profiles[i].acceleration);
      else
        ASSERT_EQ(PP_HARDWAREACCELERATION_NONE, profiles[i].acceleration);
    }
  }
}

TEST_F(VideoEncoderResourceTest, InitializeFailure) {
  {
    // Verify the initialize callback is called in case of failure.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_BGRA, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_BGRA, input_format);
    ASSERT_EQ(size.width, input_visible_size.width);
    ASSERT_EQ(size.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    SendInitializeReply(params, PP_ERROR_NOTSUPPORTED, kVideoFrameCount,
                        kFrameSize);
    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_ERROR_NOTSUPPORTED, cb.result());
  }
  {
    // Verify the initialize callback is called in case of error
    // notification.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_BGRA, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_BGRA, input_format);
    ASSERT_EQ(kFrameSize.width, input_visible_size.width);
    ASSERT_EQ(kFrameSize.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    ResourceMessageCallParams error_params(encoder.get(), 0);
    SendNotifyError(error_params, PP_ERROR_FAILED);
    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }
  {
    // Verify that calling initialize twice fails the second time if
    // we haven't received a response yet.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_BGRA, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_BGRA, input_format);
    ASSERT_EQ(size.width, input_visible_size.width);
    ASSERT_EQ(size.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_BGRA, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_ERROR_INPROGRESS, result);

    ResourceMessageCallParams error_params(encoder.get(), 0);
    SendNotifyError(error_params, PP_ERROR_FAILED);
    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }
}

TEST_F(VideoEncoderResourceTest, InitializeSuccess) {
  {
    // Verify the initialize callback is called when initialization is
    // successful.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    const uint32_t kBitrate = 420000;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_I420, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_I420, input_format);
    ASSERT_EQ(kFrameSize.width, input_visible_size.width);
    ASSERT_EQ(kFrameSize.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    SendInitializeReply(params, PP_OK, kVideoFrameCount, kFrameSize);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_OK, cb.result());
  }
  {
    // Verify that calling initialize a second time, after it already
    // succeeded, fails.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    const uint32_t kBitrate = 420000;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_I420, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_I420, input_format);
    ASSERT_EQ(kFrameSize.width, input_visible_size.width);
    ASSERT_EQ(kFrameSize.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    SendInitializeReply(params, PP_OK, kVideoFrameCount, kFrameSize);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_OK, cb.result());

    result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_I420, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_ERROR_FAILED, result);
  }
  {
    // Verify the sending the bitstream buffers details makes them
    // available through the API. the right values.
    LockingResourceReleaser encoder(CreateEncoder());
    ResourceMessageCallParams params;
    PP_Size size = kFrameSize;
    const uint32_t kBitrate = 420000;
    MockCompletionCallback cb;
    int32_t result = encoder_iface()->Initialize(
        encoder.get(), PP_VIDEOFRAME_FORMAT_I420, &size,
        PP_VIDEOPROFILE_H264MAIN, kBitrate,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    PP_VideoFrame_Format input_format;
    PP_Size input_visible_size;
    PP_VideoProfile output_profile;
    uint32_t bitrate;
    PP_HardwareAcceleration acceleration;
    ASSERT_TRUE(CheckInitializeMsg(&params, &input_format, &input_visible_size,
                                   &output_profile, &bitrate, &acceleration));
    ASSERT_EQ(PP_VIDEOFRAME_FORMAT_I420, input_format);
    ASSERT_EQ(kFrameSize.width, input_visible_size.width);
    ASSERT_EQ(kFrameSize.height, input_visible_size.height);
    ASSERT_EQ(kBitrate, bitrate);
    ASSERT_EQ(PP_VIDEOPROFILE_H264MAIN, output_profile);
    ASSERT_EQ(PP_HARDWAREACCELERATION_WITHFALLBACK, acceleration);

    SendInitializeReply(params, PP_OK, kVideoFrameCount, kFrameSize);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_OK, cb.result());

    PP_Size coded_size;
    ASSERT_EQ(PP_OK, CallGetFrameCodedSize(encoder.get(), &coded_size));
    ASSERT_EQ(kFrameSize.width, coded_size.width);
    ASSERT_EQ(kFrameSize.height, coded_size.height);
    ASSERT_EQ(static_cast<int32_t>(kVideoFrameCount),
              CallGetFramesRequired(encoder.get()));
  }
}

TEST_F(VideoEncoderResourceTest, Uninitialized) {
  // Operations on uninitialized encoders should fail.
  LockingResourceReleaser encoder(CreateEncoder());

  ASSERT_EQ(PP_ERROR_FAILED, CallGetFramesRequired(encoder.get()));

  PP_Size size;
  ASSERT_EQ(PP_ERROR_FAILED, CallGetFrameCodedSize(encoder.get(), &size));

  MockCompletionCallback uncalled_cb;
  PP_Resource video_frame = 0;
  ASSERT_EQ(PP_ERROR_FAILED,
            CallGetVideoFrame(encoder.get(), &video_frame, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
  ASSERT_EQ(0, video_frame);

  ASSERT_EQ(PP_ERROR_FAILED,
            CallEncode(encoder.get(), video_frame, PP_FALSE, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  PP_BitstreamBuffer bitstream_buffer;
  ASSERT_EQ(
      PP_ERROR_FAILED,
      CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  ResourceMessageCallParams params;
  uint32_t buffer_id;
  CallRecycleBitstreamBuffer(encoder.get(), bitstream_buffer);
  ASSERT_FALSE(CheckRecycleBitstreamBufferMsg(&params, &buffer_id));

  uint32_t bitrate, framerate;
  CallRequestEncodingParametersChange(encoder.get(), 0, 0);
  ASSERT_FALSE(
      CheckRequestEncodingParametersChangeMsg(&params, &bitrate, &framerate));
}

TEST_F(VideoEncoderResourceTest, InitializeAndGetVideoFrame) {
  // Verify that we can pull the right number of video frames before
  // the proxy makes us wait.
  LockingResourceReleaser encoder(CreateAndInitializeEncoder());
  ResourceMessageCallParams params;
  std::vector<PP_Resource> video_frames;
  MockCompletionCallback get_frame_cb;

  video_frames.resize(kVideoFrameCount + 1);

  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetVideoFrame(encoder.get(), &video_frames[0], &get_frame_cb));
  ASSERT_FALSE(get_frame_cb.called());
  ASSERT_TRUE(CheckGetVideoFramesMsg(&params));

  uint32_t frame_length = kFrameSize.width * kFrameSize.height * 2;
  CreateVideoFramesSharedMemory(frame_length, kVideoFrameCount);
  SendGetVideoFramesReply(params, kVideoFrameCount, frame_length, kFrameSize);

  for (uint32_t i = 1; i < kVideoFrameCount; ++i) {
    get_frame_cb.Reset();
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallGetVideoFrame(encoder.get(), &video_frames[i], &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());
    ASSERT_TRUE(CheckIsVideoFrame(video_frames[i]));
  }

  get_frame_cb.Reset();
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetVideoFrame(encoder.get(), &video_frames[kVideoFrameCount],
                              &get_frame_cb));
  ASSERT_FALSE(get_frame_cb.called());

  MockCompletionCallback get_frame_fail_cb;
  ASSERT_EQ(PP_ERROR_INPROGRESS,
            CallGetVideoFrame(encoder.get(), &video_frames[kVideoFrameCount],
                              &get_frame_fail_cb));
  ASSERT_FALSE(get_frame_fail_cb.called());

  // Unblock the GetVideoFrame callback by freeing up a frame.
  MockCompletionCallback encode_cb;
  ASSERT_EQ(
      PP_OK_COMPLETIONPENDING,
      CallCompleteEncode(encoder.get(), video_frames[0], PP_FALSE, &encode_cb));
  ASSERT_TRUE(encode_cb.called());
  ASSERT_EQ(PP_OK, encode_cb.result());
  ASSERT_TRUE(get_frame_cb.called());

  CallClose(encoder.get());
}

TEST_F(VideoEncoderResourceTest, Encode) {
  // Check Encode() calls into the renderer.
  LockingResourceReleaser encoder(CreateAndInitializeEncoder());
  PP_Resource video_frame;
  MockCompletionCallback get_frame_cb;

  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallFirstGetVideoFrame(encoder.get(), &video_frame, &get_frame_cb));
  ASSERT_TRUE(get_frame_cb.called());
  ASSERT_EQ(PP_OK, get_frame_cb.result());

  MockCompletionCallback encode_cb;
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallEncode(encoder.get(), video_frame, PP_TRUE, &encode_cb));
  ASSERT_FALSE(encode_cb.called());
  ASSERT_FALSE(CheckIsVideoFrameValid(video_frame));

  ResourceMessageCallParams params;
  uint32_t frame_id;
  bool force_frame;
  ASSERT_TRUE(CheckEncodeMsg(&params, &frame_id, &force_frame));

  SendEncodeReply(params, frame_id);

  ASSERT_TRUE(encode_cb.called());
  ASSERT_EQ(PP_OK, encode_cb.result());
}

TEST_F(VideoEncoderResourceTest, EncodeAndGetVideoFrame) {
  // Check the encoding loop works well.
  LockingResourceReleaser encoder(CreateAndInitializeEncoder());
  ResourceMessageCallParams params;
  PP_Resource video_frame;
  MockCompletionCallback get_frame_cb, encode_cb;

  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallFirstGetVideoFrame(encoder.get(), &video_frame, &get_frame_cb));
  ASSERT_TRUE(get_frame_cb.called());
  ASSERT_EQ(PP_OK, get_frame_cb.result());

  for (uint32_t i = 1; i < 20 * kVideoFrameCount; ++i) {
    encode_cb.Reset();
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallCompleteEncode(encoder.get(), video_frame, PP_FALSE, &encode_cb));
    ASSERT_TRUE(encode_cb.called());
    ASSERT_EQ(PP_OK, encode_cb.result());

    get_frame_cb.Reset();
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetVideoFrame(encoder.get(), &video_frame, &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());
    ASSERT_TRUE(CheckIsVideoFrame(video_frame));
  }

  ASSERT_EQ(
      PP_OK_COMPLETIONPENDING,
      CallCompleteEncode(encoder.get(), video_frame, PP_FALSE, &encode_cb));
  ASSERT_TRUE(encode_cb.called());
  ASSERT_EQ(PP_OK, encode_cb.result());
}

TEST_F(VideoEncoderResourceTest, GetBitstreamBuffer) {
  {
    // Verify that the GetBitstreamBuffer callback is fired whenever the
    // renderer signals a buffer is available.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));
    ASSERT_FALSE(get_bitstream_buffer_cb.called());

    ResourceMessageCallParams buffer_params(encoder.get(), 0);
    SendBitstreamBufferReady(buffer_params, 0, 10, true);

    ASSERT_TRUE(get_bitstream_buffer_cb.called());
    ASSERT_EQ(PP_OK, get_bitstream_buffer_cb.result());
    ASSERT_EQ(10U, bitstream_buffer.size);
    ASSERT_EQ(PP_TRUE, bitstream_buffer.key_frame);
  }
  {
    // Verify that calling GetBitstreamBuffer a second time, while the
    // first callback hasn't been fired fails.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));
    ASSERT_FALSE(get_bitstream_buffer_cb.called());

    ASSERT_EQ(PP_ERROR_INPROGRESS,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));
    ASSERT_FALSE(get_bitstream_buffer_cb.called());

    ResourceMessageCallParams buffer_params(encoder.get(), 0);
    SendBitstreamBufferReady(buffer_params, 0, 10, true);
  }
}

TEST_F(VideoEncoderResourceTest, RecycleBitstreamBuffer) {
  // Verify that we signal the renderer that a bitstream buffer has been
  // recycled.
  LockingResourceReleaser encoder(CreateAndInitializeEncoder());

  MockCompletionCallback get_bitstream_buffer_cb;
  PP_BitstreamBuffer bitstream_buffer;
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                   &get_bitstream_buffer_cb));
  ASSERT_FALSE(get_bitstream_buffer_cb.called());

  ResourceMessageCallParams buffer_params(encoder.get(), 0);
  SendBitstreamBufferReady(buffer_params, kBitstreamBufferCount - 1, 10, true);

  ASSERT_TRUE(get_bitstream_buffer_cb.called());
  ASSERT_EQ(PP_OK, get_bitstream_buffer_cb.result());

  CallRecycleBitstreamBuffer(encoder.get(), bitstream_buffer);

  ResourceMessageCallParams recycle_params;
  uint32_t buffer_id;
  ASSERT_TRUE(CheckRecycleBitstreamBufferMsg(&recycle_params, &buffer_id));
  ASSERT_EQ(kBitstreamBufferCount - 1, buffer_id);
}

TEST_F(VideoEncoderResourceTest, RequestEncodingParametersChange) {
  // Check encoding parameter changes are correctly sent to the
  // renderer.
  LockingResourceReleaser encoder(CreateAndInitializeEncoder());

  CallRequestEncodingParametersChange(encoder.get(), 1, 2);
  ResourceMessageCallParams params;
  uint32_t bitrate, framerate;
  ASSERT_TRUE(
      CheckRequestEncodingParametersChangeMsg(&params, &bitrate, &framerate));
  ASSERT_EQ(1U, bitrate);
  ASSERT_EQ(2U, framerate);
}

TEST_F(VideoEncoderResourceTest, NotifyError) {
  {
    // Check an error from the encoder aborts GetVideoFrame and
    // GetBitstreamBuffer callbacks.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_frame_cb;
    PP_Resource video_frame;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetVideoFrame(encoder.get(), &video_frame, &get_frame_cb));
    ASSERT_FALSE(get_frame_cb.called());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));

    ResourceMessageCallParams error_params(encoder.get(), 0);
    SendNotifyError(error_params, PP_ERROR_FAILED);

    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, get_frame_cb.result());
    ASSERT_TRUE(get_bitstream_buffer_cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, get_bitstream_buffer_cb.result());
  }
  {
    // Check an error from the encoder aborts Encode and GetBitstreamBuffer
    // callbacks.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_frame_cb, encode_cb1, encode_cb2;
    PP_Resource video_frame1, video_frame2;
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallFirstGetVideoFrame(encoder.get(), &video_frame1, &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());

    get_frame_cb.Reset();
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallFirstGetVideoFrame(encoder.get(), &video_frame2, &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());

    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallEncode(encoder.get(), video_frame1, PP_FALSE, &encode_cb1));
    ASSERT_FALSE(encode_cb1.called());
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallEncode(encoder.get(), video_frame2, PP_FALSE, &encode_cb2));
    ASSERT_FALSE(encode_cb2.called());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));

    ResourceMessageCallParams error_params(encoder.get(), 0);
    SendNotifyError(error_params, PP_ERROR_FAILED);

    ASSERT_TRUE(encode_cb1.called());
    ASSERT_EQ(PP_ERROR_FAILED, encode_cb1.result());
    ASSERT_TRUE(encode_cb2.called());
    ASSERT_EQ(PP_ERROR_FAILED, encode_cb2.result());
    ASSERT_TRUE(get_bitstream_buffer_cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, get_bitstream_buffer_cb.result());
  }
}

TEST_F(VideoEncoderResourceTest, Close) {
  {
    // Check closing the encoder aborts GetVideoFrame and
    // GetBitstreamBuffer callbacks.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_frame_cb;
    PP_Resource video_frame;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetVideoFrame(encoder.get(), &video_frame, &get_frame_cb));
    ASSERT_FALSE(get_frame_cb.called());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));

    CallClose(encoder.get());

    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_ERROR_ABORTED, get_frame_cb.result());
    ASSERT_TRUE(get_bitstream_buffer_cb.called());
    ASSERT_EQ(PP_ERROR_ABORTED, get_bitstream_buffer_cb.result());
  }
  {
    // Check closing the encoder aborts Encode and GetBitstreamBuffer
    // callbacks.
    LockingResourceReleaser encoder(CreateAndInitializeEncoder());

    MockCompletionCallback get_frame_cb, encode_cb1, encode_cb2;
    PP_Resource video_frame1, video_frame2;
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallFirstGetVideoFrame(encoder.get(), &video_frame1, &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());

    get_frame_cb.Reset();
    ASSERT_EQ(
        PP_OK_COMPLETIONPENDING,
        CallFirstGetVideoFrame(encoder.get(), &video_frame2, &get_frame_cb));
    ASSERT_TRUE(get_frame_cb.called());
    ASSERT_EQ(PP_OK, get_frame_cb.result());

    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallEncode(encoder.get(), video_frame1, PP_FALSE, &encode_cb1));
    ASSERT_FALSE(encode_cb1.called());
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallEncode(encoder.get(), video_frame2, PP_FALSE, &encode_cb2));
    ASSERT_FALSE(encode_cb2.called());

    MockCompletionCallback get_bitstream_buffer_cb;
    PP_BitstreamBuffer bitstream_buffer;
    ASSERT_EQ(PP_OK_COMPLETIONPENDING,
              CallGetBitstreamBuffer(encoder.get(), &bitstream_buffer,
                                     &get_bitstream_buffer_cb));

    CallClose(encoder.get());

    ASSERT_TRUE(encode_cb1.called());
    ASSERT_EQ(PP_ERROR_ABORTED, encode_cb1.result());
    ASSERT_TRUE(encode_cb2.called());
    ASSERT_EQ(PP_ERROR_ABORTED, encode_cb2.result());
    ASSERT_TRUE(get_bitstream_buffer_cb.called());
    ASSERT_EQ(PP_ERROR_ABORTED, get_bitstream_buffer_cb.result());

    // Verify that a remaining encode response from the renderer is
    // discarded.
    ResourceMessageCallParams params;
    uint32_t frame_id;
    bool force_frame;
    ASSERT_TRUE(CheckEncodeMsg(&params, &frame_id, &force_frame));
    SendEncodeReply(params, frame_id);
  }
}

}  // namespace proxy
}  // namespace ppapi
