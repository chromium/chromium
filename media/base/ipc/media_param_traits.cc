// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ipc/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "ipc/ipc_message_utils.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_point.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

using media::AudioParameters;
using media::AudioLatency;
using media::ChannelLayout;

namespace IPC {

void ParamTraits<AudioParameters>::Write(base::Pickle* m,
                                         const AudioParameters& p) {
  WriteParam(m, p.format());
  WriteParam(m, p.channel_layout());
  WriteParam(m, p.sample_rate());
  WriteParam(m, p.frames_per_buffer());
  WriteParam(m, p.channels());
  WriteParam(m, p.effects());
  WriteParam(m, p.mic_positions());
  WriteParam(m, p.latency_tag());
  WriteParam(m, p.hardware_capabilities());
}

bool ParamTraits<AudioParameters>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        AudioParameters* r) {
  AudioParameters::Format format;
  ChannelLayout channel_layout;
  int sample_rate, frames_per_buffer, channels, effects;
  std::vector<media::Point> mic_positions;
  AudioLatency::Type latency_tag;
  std::optional<media::AudioParameters::HardwareCapabilities>
      hardware_capabilities;

  if (!ReadParam(m, iter, &format) || !ReadParam(m, iter, &channel_layout) ||
      !ReadParam(m, iter, &sample_rate) ||
      !ReadParam(m, iter, &frames_per_buffer) ||
      !ReadParam(m, iter, &channels) || !ReadParam(m, iter, &effects) ||
      !ReadParam(m, iter, &mic_positions) ||
      !ReadParam(m, iter, &latency_tag) ||
      !ReadParam(m, iter, &hardware_capabilities)) {
    return false;
  }

  if (hardware_capabilities) {
    *r = AudioParameters(format, {channel_layout, channels}, sample_rate,
                         frames_per_buffer, *hardware_capabilities);
  } else {
    *r = AudioParameters(format, {channel_layout, channels}, sample_rate,
                         frames_per_buffer);
  }

  r->set_effects(effects);
  r->set_mic_positions(mic_positions);
  r->set_latency_tag(latency_tag);

  return r->IsValid();
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

void ParamTraits<AudioParameters::HardwareCapabilities>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.min_frames_per_buffer);
  WriteParam(m, p.max_frames_per_buffer);
  WriteParam(m, p.bitstream_formats);
  WriteParam(m, p.require_encapsulation);
  WriteParam(m, p.require_audio_offload);
}

bool ParamTraits<AudioParameters::HardwareCapabilities>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  int bitstream_formats;
  bool require_encapsulation;
  int max_frames_per_buffer;
  int min_frames_per_buffer;
  bool require_audio_offload;
  if (!ReadParam(m, iter, &min_frames_per_buffer) ||
      !ReadParam(m, iter, &max_frames_per_buffer) ||
      !ReadParam(m, iter, &bitstream_formats) ||
      !ReadParam(m, iter, &require_encapsulation) ||
      !ReadParam(m, iter, &require_audio_offload)) {
    return false;
  }
#if BUILDFLAG(IS_WIN)
  if (require_audio_offload &&
      !base::FeatureList::IsEnabled(media::kAudioOffload)) {
    return false;
  }
#endif
  r->min_frames_per_buffer = min_frames_per_buffer;
  r->max_frames_per_buffer = max_frames_per_buffer;
  r->bitstream_formats = bitstream_formats;
  r->require_encapsulation = require_encapsulation;
  r->require_audio_offload = require_audio_offload;
  return true;
}

void ParamTraits<AudioParameters::HardwareCapabilities>::Log(
    const param_type& p,
    std::string* l) {
  l->append(base::StringPrintf("<AudioParameters::HardwareCapabilities>"));
}

template <>
struct ParamTraits<media::EncryptionPattern> {
  typedef media::EncryptionPattern param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

void ParamTraits<media::EncryptionPattern>::Write(base::Pickle* m,
                                                  const param_type& p) {
  WriteParam(m, p.crypt_byte_block());
  WriteParam(m, p.skip_byte_block());
}

bool ParamTraits<media::EncryptionPattern>::Read(const base::Pickle* m,
                                                 base::PickleIterator* iter,
                                                 param_type* r) {
  uint32_t crypt_byte_block, skip_byte_block;
  if (!ReadParam(m, iter, &crypt_byte_block) ||
      !ReadParam(m, iter, &skip_byte_block)) {
    return false;
  }

  *r = media::EncryptionPattern(crypt_byte_block, skip_byte_block);
  return true;
}

void ParamTraits<media::EncryptionPattern>::Log(const param_type& p,
                                                std::string* l) {
  l->append(base::StringPrintf("<EncryptionPattern>"));
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC
