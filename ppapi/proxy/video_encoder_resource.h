// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_
#define PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/media_stream_buffer_manager.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_encoder_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class VideoFrameResource;

class PPAPI_PROXY_EXPORT VideoEncoderResource
    : public PluginResource,
      public thunk::PPB_VideoEncoder_API,
      public ppapi::MediaStreamBufferManager::Delegate {
 public:
  VideoEncoderResource(Connection connection, PP_Instance instance);
  ~VideoEncoderResource() override;

  thunk::PPB_VideoEncoder_API* AsPPB_VideoEncoder_API() override;

 private:
  struct ShmBuffer {
    ShmBuffer(uint32_t id, base::WritableSharedMemoryMapping mapping);
    ~ShmBuffer();

    // Index of the buffer in the vector. Buffers have the same id in
    // the plugin and the host.
    uint32_t id;
    base::WritableSharedMemoryMapping mapping;
  };

  struct BitstreamBuffer {
    BitstreamBuffer(uint32_t id, uint32_t size, bool key_frame);
    ~BitstreamBuffer();

    // Index of the buffer in the vector. Same as ShmBuffer::id.
    uint32_t id;
    uint32_t size;
    bool key_frame;
  };

  // PPB_VideoEncoder_API implementation.
  int32_t GetSupportedProfiles(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetSupportedProfiles0_1(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Initialize(PP_VideoFrame_Format input_format,
                     const PP_Size* input_visible_size,
                     PP_VideoProfile output_profile,
                     uint32_t initial_bitrate,
                     PP_HardwareAcceleration acceleration,
                     const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetFramesRequired() override;
  int32_t GetFrameCodedSize(PP_Size* size) override;
  int32_t GetVideoFrame(
      PP_Resource* video_frame,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Encode(PP_Resource video_frame,
                 PP_Bool force_keyframe,
                 const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetBitstreamBuffer(
      PP_BitstreamBuffer* picture,
      const scoped_refptr<TrackedCallback>& callback) override;
  void RecycleBitstreamBuffer(const PP_BitstreamBuffer* picture) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Close() override;

  // PluginResource implementation.
  void OnReplyReceived(const ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override;

  // Reply message handlers for operations that are done in the host.
  void OnPluginMsgGetSupportedProfilesReply(
      const PP_ArrayOutput& output,
      bool version0_1,
      const ResourceMessageReplyParams& params,
      const std::vector<PP_VideoProfileDescription>& profiles);
  void OnPluginMsgInitializeReply(const ResourceMessageReplyParams& params,
                                  uint32_t input_frame_count,
                                  const PP_Size& input_coded_size);
  void OnPluginMsgGetVideoFramesReply(const ResourceMessageReplyParams& params,
                                      uint32_t frame_count,
                                      uint32_t frame_length,
                                      const PP_Size& frame_size);
  void OnPluginMsgEncodeReply(PP_Resource video_frame,
                              const ResourceMessageReplyParams& params,
                              uint32_t frame_id);

  // Unsolicited reply message handlers.
  void OnPluginMsgBitstreamBuffers(const ResourceMessageReplyParams& params,
                                   uint32_t buffer_length);
  void OnPluginMsgBitstreamBufferReady(const ResourceMessageReplyParams& params,
                                       uint32_t buffer_id,
                                       uint32_t buffer_size,
                                       bool key_frame);
  void OnPluginMsgNotifyError(const ResourceMessageReplyParams& params,
                              int32_t error);

  // Internal utility functions.
  void NotifyError(int32_t error);
  void TryWriteVideoFrame();
  void WriteBitstreamBuffer(const BitstreamBuffer& buffer);
  void ReleaseFrames();

  bool initialized_;
  bool closed_;
  int32_t encoder_last_error_;

  int32_t input_frame_count_;
  PP_Size input_coded_size_;

  MediaStreamBufferManager buffer_manager_;

  typedef std::map<PP_Resource, scoped_refptr<VideoFrameResource> >
      VideoFrameMap;
  VideoFrameMap video_frames_;

  std::vector<std::unique_ptr<ShmBuffer>> shm_buffers_;

  base::circular_deque<BitstreamBuffer> available_bitstream_buffers_;
  using BitstreamBufferMap = std::map<void*, uint32_t>;
  BitstreamBufferMap bitstream_buffer_map_;

  scoped_refptr<TrackedCallback> get_supported_profiles_callback_;
  scoped_refptr<TrackedCallback> initialize_callback_;
  scoped_refptr<TrackedCallback> get_video_frame_callback_;
  PP_Resource* get_video_frame_data_;

  using EncodeMap = std::map<PP_Resource, scoped_refptr<TrackedCallback>>;
  EncodeMap encode_callbacks_;

  scoped_refptr<TrackedCallback> get_bitstream_buffer_callback_;
  PP_BitstreamBuffer* get_bitstream_buffer_data_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_ENCODER_RESOURCE_H_
