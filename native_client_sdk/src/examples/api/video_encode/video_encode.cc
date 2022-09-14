// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/media_stream_video_track.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/cpp/video_encoder.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage, min and max can be
// #define-d to something else.
#ifdef WIN32
#undef min
#undef max
#undef PostMessage
#endif

// Use assert as a makeshift CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>

#define fourcc(a, b, c, d)                                               \
  (((uint32_t)(a) << 0) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | \
   ((uint32_t)(d) << 24))

namespace {

double clamp(double min, double max, double value) {
  return std::max(std::min(value, max), min);
}

std::string ToUpperString(const std::string& str) {
  std::string ret;
  for (uint32_t i = 0; i < str.size(); i++)
    ret.push_back(static_cast<char>(toupper(str[i])));
  return ret;
}

// IVF container writer. It is possible to parse H264 bitstream using
// NAL units but for VP8 we need a container to at least find encoded
// pictures as well as the picture sizes.
class IVFWriter {
 public:
  IVFWriter() {}
  ~IVFWriter() {}

  uint32_t GetFileHeaderSize() const { return 32; }
  uint32_t GetFrameHeaderSize() const { return 12; }
  uint32_t WriteFileHeader(uint8_t* mem,
                           const std::string& codec,
                           int32_t width,
                           int32_t height);
  uint32_t WriteFrameHeader(uint8_t* mem, uint64_t pts, size_t frame_size);

 private:
  void PutLE16(uint8_t* mem, int val) const {
    mem[0] = (val >> 0) & 0xff;
    mem[1] = (val >> 8) & 0xff;
  }
  void PutLE32(uint8_t* mem, int val) const {
    mem[0] = (val >> 0) & 0xff;
    mem[1] = (val >> 8) & 0xff;
    mem[2] = (val >> 16) & 0xff;
    mem[3] = (val >> 24) & 0xff;
  }
};

uint32_t IVFWriter::WriteFileHeader(uint8_t* mem,
                                    const std::string& codec,
                                    int32_t width,
                                    int32_t height) {
  mem[0] = 'D';
  mem[1] = 'K';
  mem[2] = 'I';
  mem[3] = 'F';
  PutLE16(mem + 4, 0);                               // version
  PutLE16(mem + 6, 32);                              // header size
  PutLE32(mem + 8, fourcc(codec[0], codec[1], codec[2], '0'));  // fourcc
  PutLE16(mem + 12, static_cast<uint16_t>(width));   // width
  PutLE16(mem + 14, static_cast<uint16_t>(height));  // height
  PutLE32(mem + 16, 1000);                           // rate
  PutLE32(mem + 20, 1);                              // scale
  PutLE32(mem + 24, 0xffffffff);                     // length
  PutLE32(mem + 28, 0);                              // unused

  return 32;
}

uint32_t IVFWriter::WriteFrameHeader(uint8_t* mem,
                                     uint64_t pts,
                                     size_t frame_size) {
  PutLE32(mem, (int)frame_size);
  PutLE32(mem + 4, (int)(pts & 0xFFFFFFFF));
  PutLE32(mem + 8, (int)(pts >> 32));

  return 12;
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class VideoEncoderModule : public pp::Module {
 public:
  VideoEncoderModule() : pp::Module() {}
  virtual ~VideoEncoderModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class VideoEncoderInstance : public pp::Instance {
 public:
  VideoEncoderInstance(PP_Instance instance, pp::Module* module);
  virtual ~VideoEncoderInstance();

  // pp::Instance implementation.
  virtual void HandleMessage(const pp::Var& var_message);

 private:
  void AddVideoProfile(PP_VideoProfile profile, const std::string& profile_str);
  void InitializeVideoProfiles();
  PP_VideoProfile VideoProfileFromString(const std::string& str);
  std::string VideoProfileToString(PP_VideoProfile profile);

  void ConfigureTrack();
  void OnConfiguredTrack(int32_t result);
  void ProbeEncoder();
  void OnEncoderProbed(int32_t result,
                       const std::vector<PP_VideoProfileDescription> profiles);
  void StartEncoder();
  void OnInitializedEncoder(int32_t result);
  void ScheduleNextEncode();
  void GetEncoderFrameTick(int32_t result);
  void GetEncoderFrame(const pp::VideoFrame& track_frame);
  void OnEncoderFrame(int32_t result,
                      pp::VideoFrame encoder_frame,
                      pp::VideoFrame track_frame);
  int32_t CopyVideoFrame(pp::VideoFrame dest, pp::VideoFrame src);
  void EncodeFrame(const pp::VideoFrame& frame);
  void OnEncodeDone(int32_t result);
  void OnGetBitstreamBuffer(int32_t result, PP_BitstreamBuffer buffer);
  void StartTrackFrames();
  void StopTrackFrames();
  void OnTrackFrame(int32_t result, pp::VideoFrame frame);

  void StopEncode();

  void LogError(int32_t error, const std::string& message);
  void Log(const std::string& message);

  void PostDataMessage(const void* buffer, uint32_t size);

  typedef std::map<std::string, PP_VideoProfile> VideoProfileFromStringMap;
  VideoProfileFromStringMap profile_from_string_;

  typedef std::map<PP_VideoProfile, std::string> VideoProfileToStringMap;
  VideoProfileToStringMap profile_to_string_;

  bool is_encoding_;
  bool is_encode_ticking_;
  bool is_receiving_track_frames_;

  pp::VideoEncoder video_encoder_;
  pp::MediaStreamVideoTrack video_track_;
  pp::CompletionCallbackFactory<VideoEncoderInstance> callback_factory_;

  PP_VideoProfile video_profile_;
  PP_VideoFrame_Format frame_format_;

  pp::Size requested_size_;
  pp::Size frame_size_;
  pp::Size encoder_size_;
  uint32_t encoded_frames_;

  std::deque<uint64_t> frames_timestamps_;

  pp::VideoFrame current_track_frame_;

  IVFWriter ivf_writer_;

  PP_Time last_encode_tick_;
};

VideoEncoderInstance::VideoEncoderInstance(PP_Instance instance,
                                           pp::Module* module)
    : pp::Instance(instance),
      is_encoding_(false),
      is_encode_ticking_(false),
      callback_factory_(this),
#if defined(USE_VP8_INSTEAD_OF_H264)
      video_profile_(PP_VIDEOPROFILE_VP8_ANY),
#else
      video_profile_(PP_VIDEOPROFILE_H264MAIN),
#endif
      frame_format_(PP_VIDEOFRAME_FORMAT_I420),
      encoded_frames_(0),
      last_encode_tick_(0) {
  InitializeVideoProfiles();
  ProbeEncoder();
}

VideoEncoderInstance::~VideoEncoderInstance() {
}

void VideoEncoderInstance::AddVideoProfile(PP_VideoProfile profile,
                                           const std::string& profile_str) {
  profile_to_string_.insert(std::make_pair(profile, profile_str));
  profile_from_string_.insert(std::make_pair(profile_str, profile));
}

void VideoEncoderInstance::InitializeVideoProfiles() {
  AddVideoProfile(PP_VIDEOPROFILE_H264BASELINE, "h264baseline");
  AddVideoProfile(PP_VIDEOPROFILE_H264MAIN, "h264main");
  AddVideoProfile(PP_VIDEOPROFILE_H264EXTENDED, "h264extended");
  AddVideoProfile(PP_VIDEOPROFILE_H264HIGH, "h264high");
  AddVideoProfile(PP_VIDEOPROFILE_H264HIGH10PROFILE, "h264high10");
  AddVideoProfile(PP_VIDEOPROFILE_H264HIGH422PROFILE, "h264high422");
  AddVideoProfile(PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE,
                  "h264high444predictive");
  AddVideoProfile(PP_VIDEOPROFILE_H264SCALABLEBASELINE, "h264scalablebaseline");
  AddVideoProfile(PP_VIDEOPROFILE_H264SCALABLEHIGH, "h264scalablehigh");
  AddVideoProfile(PP_VIDEOPROFILE_H264STEREOHIGH, "h264stereohigh");
  AddVideoProfile(PP_VIDEOPROFILE_H264MULTIVIEWHIGH, "h264multiviewhigh");
  AddVideoProfile(PP_VIDEOPROFILE_VP8_ANY, "vp8");
  AddVideoProfile(PP_VIDEOPROFILE_VP9_ANY, "vp9");
}

PP_VideoProfile VideoEncoderInstance::VideoProfileFromString(
    const std::string& str) {
  VideoProfileFromStringMap::iterator it = profile_from_string_.find(str);
  if (it == profile_from_string_.end())
    return PP_VIDEOPROFILE_VP8_ANY;
  return it->second;
}

std::string VideoEncoderInstance::VideoProfileToString(
    PP_VideoProfile profile) {
  VideoProfileToStringMap::iterator it = profile_to_string_.find(profile);
  if (it == profile_to_string_.end())
    return "unknown";
  return it->second;
}

void VideoEncoderInstance::ConfigureTrack() {
  if (encoder_size_.IsEmpty())
    frame_size_ = requested_size_;
  else
    frame_size_ = encoder_size_;

  int32_t attrib_list[] = {PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT,
                           frame_format_,
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH,
                           frame_size_.width(),
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT,
                           frame_size_.height(),
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE};

  video_track_.Configure(
      attrib_list,
      callback_factory_.NewCallback(&VideoEncoderInstance::OnConfiguredTrack));
}

void VideoEncoderInstance::OnConfiguredTrack(int32_t result) {
  if (result != PP_OK) {
    LogError(result, "Cannot configure track");
    return;
  }

  if (is_encoding_) {
    StartTrackFrames();
    ScheduleNextEncode();
  } else
    StartEncoder();
}

void VideoEncoderInstance::ProbeEncoder() {
  video_encoder_ = pp::VideoEncoder(this);
  video_encoder_.GetSupportedProfiles(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnEncoderProbed));
}

void VideoEncoderInstance::OnEncoderProbed(
    int32_t result,
    const std::vector<PP_VideoProfileDescription> profiles) {
  pp::VarDictionary dict;
  dict.Set(pp::Var("name"), pp::Var("supportedProfiles"));
  pp::VarArray js_profiles;
  dict.Set(pp::Var("profiles"), js_profiles);

  if (result < 0) {
    LogError(result, "Cannot get supported profiles");
    PostMessage(dict);
  }

  int32_t idx = 0;
  for (uint32_t i = 0; i < profiles.size(); i++) {
    const PP_VideoProfileDescription& profile = profiles[i];
    js_profiles.Set(idx++, pp::Var(VideoProfileToString(profile.profile)));
  }
  PostMessage(dict);
}

void VideoEncoderInstance::StartEncoder() {
  video_encoder_ = pp::VideoEncoder(this);
  frames_timestamps_.clear();

  int32_t error = video_encoder_.Initialize(
      frame_format_, frame_size_, video_profile_, 2000000,
      PP_HARDWAREACCELERATION_WITHFALLBACK,
      callback_factory_.NewCallback(
          &VideoEncoderInstance::OnInitializedEncoder));
  if (error != PP_OK_COMPLETIONPENDING) {
    LogError(error, "Cannot initialize encoder");
    return;
  }
}

void VideoEncoderInstance::OnInitializedEncoder(int32_t result) {
  if (result != PP_OK) {
    LogError(result, "Encoder initialization failed");
    return;
  }

  is_encoding_ = true;
  Log("started");

  if (video_encoder_.GetFrameCodedSize(&encoder_size_) != PP_OK) {
    LogError(result, "Cannot get encoder coded frame size");
    return;
  }

  video_encoder_.GetBitstreamBuffer(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnGetBitstreamBuffer));

  if (encoder_size_ != frame_size_)
    ConfigureTrack();
  else {
    StartTrackFrames();
    ScheduleNextEncode();
  }
}

void VideoEncoderInstance::ScheduleNextEncode() {
  // Avoid scheduling more than once at a time.
  if (is_encode_ticking_)
    return;

  PP_Time now = pp::Module::Get()->core()->GetTime();
  PP_Time tick = 1.0 / 30;
  // If the callback was triggered late, we need to account for that
  // delay for the next tick.
  PP_Time delta = tick - clamp(0, tick, now - last_encode_tick_ - tick);

  pp::Module::Get()->core()->CallOnMainThread(
      delta * 1000,
      callback_factory_.NewCallback(&VideoEncoderInstance::GetEncoderFrameTick),
      0);

  last_encode_tick_ = now;
  is_encode_ticking_ = true;
}

void VideoEncoderInstance::GetEncoderFrameTick(int32_t result) {
  is_encode_ticking_ = false;

  if (is_encoding_) {
    if (!current_track_frame_.is_null()) {
      pp::VideoFrame frame = current_track_frame_;
      current_track_frame_.detach();
      GetEncoderFrame(frame);
    }
    ScheduleNextEncode();
  }
}

void VideoEncoderInstance::GetEncoderFrame(const pp::VideoFrame& track_frame) {
  video_encoder_.GetVideoFrame(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnEncoderFrame, track_frame));
}

void VideoEncoderInstance::OnEncoderFrame(int32_t result,
                                          pp::VideoFrame encoder_frame,
                                          pp::VideoFrame track_frame) {
  if (result == PP_ERROR_ABORTED) {
    video_track_.RecycleFrame(track_frame);
    return;
  }
  if (result != PP_OK) {
    video_track_.RecycleFrame(track_frame);
    LogError(result, "Cannot get video frame from video encoder");
    return;
  }

  track_frame.GetSize(&frame_size_);

  if (frame_size_ != encoder_size_) {
    video_track_.RecycleFrame(track_frame);
    LogError(PP_ERROR_FAILED, "MediaStreamVideoTrack frame size incorrect");
    return;
  }

  if (CopyVideoFrame(encoder_frame, track_frame) == PP_OK)
    EncodeFrame(encoder_frame);
  video_track_.RecycleFrame(track_frame);
}

int32_t VideoEncoderInstance::CopyVideoFrame(pp::VideoFrame dest,
                                             pp::VideoFrame src) {
  if (dest.GetDataBufferSize() < src.GetDataBufferSize()) {
    std::ostringstream oss;
    oss << "Incorrect destination video frame buffer size : "
        << dest.GetDataBufferSize() << " < " << src.GetDataBufferSize();
    LogError(PP_ERROR_FAILED, oss.str());
    return PP_ERROR_FAILED;
  }

  dest.SetTimestamp(src.GetTimestamp());
  memcpy(dest.GetDataBuffer(), src.GetDataBuffer(), src.GetDataBufferSize());
  return PP_OK;
}

void VideoEncoderInstance::EncodeFrame(const pp::VideoFrame& frame) {
  frames_timestamps_.push_back(
      static_cast<uint64_t>(frame.GetTimestamp() * 1000));
  video_encoder_.Encode(
      frame, PP_FALSE,
      callback_factory_.NewCallback(&VideoEncoderInstance::OnEncodeDone));
}

void VideoEncoderInstance::OnEncodeDone(int32_t result) {
  if (result == PP_ERROR_ABORTED)
    return;
  if (result != PP_OK)
    LogError(result, "Encode failed");
}

void VideoEncoderInstance::OnGetBitstreamBuffer(int32_t result,
                                                PP_BitstreamBuffer buffer) {
  if (result == PP_ERROR_ABORTED)
    return;
  if (result != PP_OK) {
    LogError(result, "Cannot get bitstream buffer");
    return;
  }

  encoded_frames_++;
  PostDataMessage(buffer.buffer, buffer.size);
  video_encoder_.RecycleBitstreamBuffer(buffer);

  video_encoder_.GetBitstreamBuffer(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnGetBitstreamBuffer));
}

void VideoEncoderInstance::StartTrackFrames() {
  is_receiving_track_frames_ = true;
  video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnTrackFrame));
}

void VideoEncoderInstance::StopTrackFrames() {
  is_receiving_track_frames_ = false;
  if (!current_track_frame_.is_null()) {
    video_track_.RecycleFrame(current_track_frame_);
    current_track_frame_.detach();
  }
}

void VideoEncoderInstance::OnTrackFrame(int32_t result, pp::VideoFrame frame) {
  if (result == PP_ERROR_ABORTED)
    return;

  if (!current_track_frame_.is_null()) {
    video_track_.RecycleFrame(current_track_frame_);
    current_track_frame_.detach();
  }

  if (result != PP_OK) {
    LogError(result, "Cannot get video frame from video track");
    return;
  }

  current_track_frame_ = frame;
  if (is_receiving_track_frames_)
    video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
        &VideoEncoderInstance::OnTrackFrame));
}

void VideoEncoderInstance::StopEncode() {
  video_encoder_.Close();
  StopTrackFrames();
  video_track_.Close();
  is_encoding_ = false;
  encoded_frames_ = 0;
}

//

void VideoEncoderInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_dictionary()) {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid message!"));
    return;
  }

  pp::VarDictionary dict_message(var_message);
  std::string command = dict_message.Get("command").AsString();

  if (command == "start") {
    requested_size_ = pp::Size(dict_message.Get("width").AsInt(),
                               dict_message.Get("height").AsInt());
    pp::Var var_track = dict_message.Get("track");
    if (!var_track.is_resource()) {
      LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Given track is not a resource"));
      return;
    }
    pp::Resource resource_track = var_track.AsResource();
    video_track_ = pp::MediaStreamVideoTrack(resource_track);
    video_encoder_ = pp::VideoEncoder();
    video_profile_ = VideoProfileFromString(
        dict_message.Get("profile").AsString());
    ConfigureTrack();
  } else if (command == "stop") {
    StopEncode();
    Log("stopped");
  } else {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid command!"));
  }
}

void VideoEncoderInstance::PostDataMessage(const void* buffer, uint32_t size) {
  pp::VarDictionary dictionary;

  dictionary.Set(pp::Var("name"), pp::Var("data"));

  pp::VarArrayBuffer array_buffer;
  uint8_t* data_ptr;
  uint32_t data_offset = 0;
  if (video_profile_ == PP_VIDEOPROFILE_VP8_ANY ||
      video_profile_ == PP_VIDEOPROFILE_VP9_ANY) {
    uint32_t frame_offset = 0;
    if (encoded_frames_ == 1) {
      array_buffer = pp::VarArrayBuffer(
          size + ivf_writer_.GetFileHeaderSize() +
          ivf_writer_.GetFrameHeaderSize());
      data_ptr = static_cast<uint8_t*>(array_buffer.Map());
      frame_offset = ivf_writer_.WriteFileHeader(
          data_ptr, ToUpperString(VideoProfileToString(video_profile_)),
          frame_size_.width(), frame_size_.height());
    } else {
      array_buffer = pp::VarArrayBuffer(
          size + ivf_writer_.GetFrameHeaderSize());
      data_ptr = static_cast<uint8_t*>(array_buffer.Map());
    }
    uint64_t timestamp = frames_timestamps_.front();
    frames_timestamps_.pop_front();
    data_offset =
        frame_offset +
        ivf_writer_.WriteFrameHeader(data_ptr + frame_offset, timestamp, size);
  } else {
    array_buffer = pp::VarArrayBuffer(size);
    data_ptr = static_cast<uint8_t*>(array_buffer.Map());
  }

  memcpy(data_ptr + data_offset, buffer, size);
  array_buffer.Unmap();
  dictionary.Set(pp::Var("data"), array_buffer);

  PostMessage(dictionary);
}

void VideoEncoderInstance::LogError(int32_t error, const std::string& message) {
  std::string msg("Error: ");
  msg.append(pp::Var(error).DebugString());
  msg.append(" : ");
  msg.append(message);

  Log(msg);
}

void VideoEncoderInstance::Log(const std::string& message) {
  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("name"), pp::Var("log"));
  dictionary.Set(pp::Var("message"), pp::Var(message));

  PostMessage(dictionary);
}

pp::Instance* VideoEncoderModule::CreateInstance(PP_Instance instance) {
  return new VideoEncoderInstance(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new VideoEncoderModule();
}
}  // namespace pp
