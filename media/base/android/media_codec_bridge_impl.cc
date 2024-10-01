// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/android/media_codec_bridge_impl.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/containers/heap_array.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/android/jni_hdr_metadata.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media_codec_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/MediaCodecBridgeBuilder_jni.h"
#include "media/base/android/media_jni_headers/MediaCodecBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

#define RETURN_ON_ERROR(condition)                             \
  do {                                                         \
    if (!(condition)) {                                        \
      LOG(ERROR) << "Unable to parse AAC header: " #condition; \
      return false;                                            \
    }                                                          \
  } while (0)

namespace media {
namespace {

enum {
  kBufferFlagSyncFrame = 1,    // BUFFER_FLAG_SYNC_FRAME
  kBufferFlagEndOfStream = 4,  // BUFFER_FLAG_END_OF_STREAM
  kConfigureFlagEncode = 1,    // CONFIGURE_FLAG_ENCODE
  kBitrateModeCBR = 2,         // BITRATE_MODE_CBR
  kBitrateModeVBR = 1,         // BITRATE_MODE_VBR
};

using CodecSpecificData = std::vector<uint8_t>;

// Parses |extra_data| to get info to be added to a Java MediaFormat.
bool GetCodecSpecificDataForAudio(const AudioDecoderConfig& config,
                                  CodecSpecificData* output_csd0,
                                  CodecSpecificData* output_csd1,
                                  CodecSpecificData* output_csd2,
                                  bool* output_frame_has_adts_header) {
  // It's important that the multiplication is first in this calculation to
  // reduce the precision loss due to integer truncation.
  const int64_t codec_delay_ns = base::Time::kNanosecondsPerSecond *
                                 config.codec_delay() /
                                 config.samples_per_second();
  const int64_t seek_preroll_ns = config.seek_preroll().InMicroseconds() *
                                  base::Time::kNanosecondsPerMicrosecond;

  *output_frame_has_adts_header = false;

  switch (config.codec()) {
    case AudioCodec::kVorbis: {
      if (config.extra_data().empty()) {
        return true;
      }
      const uint8_t* extra_data = config.extra_data().data();
      const size_t extra_data_size = config.extra_data().size();

      if (extra_data[0] != 2) {
        LOG(ERROR) << "Invalid number of vorbis headers before the codec "
                   << "header: " << extra_data[0];
        return false;
      }

      size_t header_length[2];
      // |total_length| keeps track of the total number of bytes before the last
      // header.
      size_t total_length = 1;
      const uint8_t* current_pos = extra_data;
      // Calculate the length of the first 2 headers.
      for (int i = 0; i < 2; ++i) {
        header_length[i] = 0;
        while (total_length < extra_data_size) {
          size_t size = *(++current_pos);
          total_length += 1 + size;
          if (total_length > 0x80000000) {
            LOG(ERROR) << "Vorbis header size too large";
            return false;
          }
          header_length[i] += size;
          if (size < 0xFF)
            break;
        }
        if (total_length >= extra_data_size) {
          LOG(ERROR) << "Invalid vorbis header size in the extra data";
          return false;
        }
      }
      current_pos++;

      // The first header is the identification header.
      output_csd0->assign(current_pos, current_pos + header_length[0]);

      // The last header is the codec header.
      output_csd1->assign(extra_data + total_length,
                          extra_data + extra_data_size);
      break;
    }
    case AudioCodec::kFLAC: {
      if (config.extra_data().empty()) {
        return true;
      }

      // According to MediaCodec spec, CSB buffer #0 for FLAC should be:
      // "fLaC", the FLAC stream marker in ASCII, followed by the STREAMINFO
      // block (the mandatory metadata block), optionally followed by any number
      // of other metadata blocks.
      output_csd0->emplace_back('f');
      output_csd0->emplace_back('L');
      output_csd0->emplace_back('a');
      output_csd0->emplace_back('C');
      // The STREAMINFO block should contain the METADATA_BLOCK_HEADER.
      // <1> last-metadata-block flag: 1
      // <7> block type: STREAMINFO (0)
      output_csd0->emplace_back(0x80);
      // <24> length of metadata to follow.
      const size_t extra_data_size = config.extra_data().size();
      DCHECK_LE(extra_data_size, static_cast<size_t>(0xffffff));
      output_csd0->emplace_back((extra_data_size & 0xff0000) >> 16);
      output_csd0->emplace_back((extra_data_size & 0x00ff00) >> 8);
      output_csd0->emplace_back(extra_data_size & 0x0000ff);
      // STREAMINFO bytes.
      output_csd0->insert(output_csd0->end(), config.extra_data().begin(),
                          config.extra_data().end());
      break;
    }
    case AudioCodec::kAAC: {
      if (config.aac_extra_data().empty()) {
        return false;
      }
      *output_csd0 = config.aac_extra_data();
      *output_frame_has_adts_header =
          config.profile() != AudioCodecProfile::kXHE_AAC;
      break;
    }
    case AudioCodec::kOpus: {
      if (config.extra_data().empty() || codec_delay_ns < 0 ||
          seek_preroll_ns < 0) {
        LOG(ERROR) << "Invalid Opus Header";
        return false;
      }

      // csd0 - Opus Header
      *output_csd0 = config.extra_data();

      // csd1 - Codec Delay
      const uint8_t* codec_delay_ns_ptr =
          reinterpret_cast<const uint8_t*>(&codec_delay_ns);
      output_csd1->assign(codec_delay_ns_ptr,
                          codec_delay_ns_ptr + sizeof(int64_t));

      // csd2 - Seek Preroll
      const uint8_t* seek_preroll_ns_ptr =
          reinterpret_cast<const uint8_t*>(&seek_preroll_ns);
      output_csd2->assign(seek_preroll_ns_ptr,
                          seek_preroll_ns_ptr + sizeof(int64_t));
      break;
    }
    default:
      if (config.extra_data().empty()) {
        return true;
      }
      LOG(ERROR) << "Unsupported audio codec encountered: "
                 << GetCodecName(config.codec());
      return false;
  }
  return true;
}

// Given |status|, return the appropriate MediaCodecResult::Codes for it. This
// is needed so that the actual MediaCodecResult object is created next to the
// call that so that the call stack points at a usable line number, not
// somewhere in this function.
MediaCodecResult::Codes ConvertToMediaCodecEnum(MediaCodecStatus status) {
  switch (status) {
    case MEDIA_CODEC_OK:
      return MediaCodecResult::Codes::kOk;
    case MEDIA_CODEC_TRY_AGAIN_LATER:
      return MediaCodecResult::Codes::kTryAgainLater;
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      return MediaCodecResult::Codes::kOutputBuffersChanged;
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      return MediaCodecResult::Codes::kOutputFormatChanged;
    case MEDIA_CODEC_NO_KEY:
      return MediaCodecResult::Codes::kNoKey;
    case MEDIA_CODEC_ERROR:
    case MEDIA_CODEC_KEY_EXPIRED:
    case MEDIA_CODEC_RESOURCE_BUSY:
    case MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION:
    case MEDIA_CODEC_SESSION_NOT_OPENED:
    case MEDIA_CODEC_UNSUPPORTED_OPERATION:
    case MEDIA_CODEC_INSUFFICIENT_SECURITY:
    case MEDIA_CODEC_FRAME_TOO_LARGE:
    case MEDIA_CODEC_LOST_STATE:
    case MEDIA_CODEC_GENERIC_OEM:
    case MEDIA_CODEC_GENERIC_PLUGIN:
    case MEDIA_CODEC_LICENSE_PARSE:
    case MEDIA_CODEC_MEDIA_FRAMEWORK:
    case MEDIA_CODEC_ZERO_SUBSAMPLES:
    case MEDIA_CODEC_UNKNOWN_CIPHER_MODE:
    case MEDIA_CODEC_PATTERN_ENCRYPTION_NOT_SUPPORTED:
      return MediaCodecResult::Codes::kError;
  }
}

// Given |status|, return an appropriate error message to be included in
// MediaCodecResult.
std::string ApplyDescriptiveMessage(MediaCodecStatus status) {
  switch (status) {
    case MEDIA_CODEC_OK:
      return "";
    case MEDIA_CODEC_TRY_AGAIN_LATER:
      return "No output buffers are available.";
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      return "The output buffers have changed, subsequent data must use the "
             "new set of output buffers.";
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      return "The output format has changed, subsequent data will follow the "
             "new format.";
    case MEDIA_CODEC_NO_KEY:
      return "The requested key was not found when trying to perform a decrypt "
             "operation.";
    case MEDIA_CODEC_ERROR:
      return "Unexpected error occurred.";
    case MEDIA_CODEC_KEY_EXPIRED:
      return "The key used for decryption is no longer valid due to license "
             "term expiration.";
    case MEDIA_CODEC_RESOURCE_BUSY:
      return "A required crypto resource was not able to be allocated while "
             "attempting the requested operation.";
    case MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION:
      return "The output protection levels supported by the device are not "
             "sufficient to meet the requirements set by the content owner in "
             "the license policy.";
    case MEDIA_CODEC_SESSION_NOT_OPENED:
      return "Decryption was attempted on a session that is not opened, which "
             "could be due to a failure to open the session, closing the "
             "session prematurely, the session being reclaimed by the resource "
             "manager, or a non-existent session id.";
    case MEDIA_CODEC_UNSUPPORTED_OPERATION:
      return "An operation was attempted that could not be supported by the "
             "crypto system of the device in its current configuration.";
    case MEDIA_CODEC_INSUFFICIENT_SECURITY:
      return "The security level of the device is not sufficient to meet the "
             "requirements set by the content owner in the license policy.";
    case MEDIA_CODEC_FRAME_TOO_LARGE:
      return "The video frame being decrypted exceeds the size of the device's "
             "protected output buffers.";
    case MEDIA_CODEC_LOST_STATE:
      return "The session state has been invalidated.";
    case MEDIA_CODEC_GENERIC_OEM:
      return "Unexpected error reported by the device OEM subsystem.";
    case MEDIA_CODEC_GENERIC_PLUGIN:
      return "Unexpected internal failure in MediaDrm/MediaCrypto.";
    case MEDIA_CODEC_LICENSE_PARSE:
      return "The license response was empty, fields are missing or otherwise "
             "unable to be parsed or decrypted.";
    case MEDIA_CODEC_MEDIA_FRAMEWORK:
      return "Failure in the media framework.";
    case MEDIA_CODEC_ZERO_SUBSAMPLES:
      return "Input buffer provided with 0 subsamples.";
    case MEDIA_CODEC_UNKNOWN_CIPHER_MODE:
      return "Cipher mode is not supported.";
    case MEDIA_CODEC_PATTERN_ENCRYPTION_NOT_SUPPORTED:
      return "Pattern encryption only supported for 'cbcs' scheme (CBC mode).";
  }
}

}  // namespace

VideoCodecConfig::VideoCodecConfig() = default;
VideoCodecConfig::~VideoCodecConfig() = default;

// static
std::unique_ptr<MediaCodecBridge> MediaCodecBridgeImpl::CreateAudioDecoder(
    const AudioDecoderConfig& config,
    const JavaRef<jobject>& media_crypto,
    base::RepeatingClosure on_buffers_available_cb) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString()
           << " media_crypto:" << media_crypto.obj();

  const std::string mime = MediaCodecUtil::CodecToAndroidMimeType(
      config.codec(), config.target_output_sample_format());

  if (mime.empty())
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);

  const int channel_count =
      ChannelLayoutToChannelCount(config.channel_layout());

  CodecSpecificData csd0, csd1, csd2;
  bool output_frame_has_adts_header;
  if (!GetCodecSpecificDataForAudio(config, &csd0, &csd1, &csd2,
                                    &output_frame_has_adts_header)) {
    return nullptr;
  }

  ScopedJavaLocalRef<jbyteArray> j_csd0 = ToJavaByteArray(env, csd0);
  ScopedJavaLocalRef<jbyteArray> j_csd1 = ToJavaByteArray(env, csd1);
  ScopedJavaLocalRef<jbyteArray> j_csd2 = ToJavaByteArray(env, csd2);

  ScopedJavaGlobalRef<jobject> j_bridge(
      Java_MediaCodecBridgeBuilder_createAudioDecoder(
          env, j_mime, media_crypto, config.samples_per_second(), channel_count,
          j_csd0, j_csd1, j_csd2, output_frame_has_adts_header,
          !!on_buffers_available_cb));

  if (j_bridge.is_null())
    return nullptr;

  return base::WrapUnique(
      new MediaCodecBridgeImpl(CodecType::kAny, std::move(j_bridge),
                               std::move(on_buffers_available_cb)));
}

// static
std::unique_ptr<MediaCodecBridge> MediaCodecBridgeImpl::CreateVideoDecoder(
    const VideoCodecConfig& config) {
  const std::string mime = MediaCodecUtil::CodecToAndroidMimeType(config.codec);
  if (mime.empty())
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  auto j_mime = ConvertUTF8ToJavaString(env, mime);
  auto j_csd0 = ToJavaByteArray(env, config.csd0);
  auto j_csd1 = ToJavaByteArray(env, config.csd1);

  std::unique_ptr<JniHdrMetadata> jni_hdr_metadata;
  if (config.hdr_metadata.has_value()) {
    jni_hdr_metadata = std::make_unique<JniHdrMetadata>(
        config.container_color_space, config.hdr_metadata.value());
  }
  auto j_hdr_metadata = jni_hdr_metadata ? jni_hdr_metadata->obj() : nullptr;
  auto j_decoder_name = ConvertUTF8ToJavaString(env, config.name);

  ScopedJavaGlobalRef<jobject> j_bridge(
      Java_MediaCodecBridgeBuilder_createVideoDecoder(
          env, j_mime, static_cast<int>(config.codec_type), config.media_crypto,
          config.initial_expected_coded_size.width(),
          config.initial_expected_coded_size.height(), config.surface, j_csd0,
          j_csd1, j_hdr_metadata, /*allowAdaptivePlayback=*/true,
          /*useAsyncApi=*/!!config.on_buffers_available_cb,
          /*useBlockModel=*/config.use_block_model, j_decoder_name,
          config.profile));
  if (j_bridge.is_null())
    return nullptr;

  return base::WrapUnique(new MediaCodecBridgeImpl(
      config.codec_type, std::move(j_bridge), config.on_buffers_available_cb));
}

// static
std::unique_ptr<MediaCodecBridge> MediaCodecBridgeImpl::CreateVideoEncoder(
    VideoCodec codec,
    const gfx::Size& size,
    int bit_rate,
    int frame_rate,
    int i_frame_interval,
    int color_format) {
  const std::string mime = MediaCodecUtil::CodecToAndroidMimeType(codec);
  if (mime.empty())
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaGlobalRef<jobject> j_bridge(
      Java_MediaCodecBridgeBuilder_createVideoEncoder(
          env, j_mime, size.width(), size.height(), kBitrateModeCBR, bit_rate,
          frame_rate, i_frame_interval, color_format));

  if (j_bridge.is_null())
    return nullptr;

  return base::WrapUnique(
      new MediaCodecBridgeImpl(CodecType::kAny, std::move(j_bridge)));
}

// static
void MediaCodecBridgeImpl::SetupCallbackHandlerForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_createCallbackHandlerForTesting(env);
}

MediaCodecBridgeImpl::MediaCodecBridgeImpl(
    CodecType codec_type,
    ScopedJavaGlobalRef<jobject> j_bridge,
    base::RepeatingClosure on_buffers_available_cb)
    : codec_type_(codec_type),
      on_buffers_available_cb_(std::move(on_buffers_available_cb)),
      j_bridge_(std::move(j_bridge)) {
  DCHECK(!j_bridge_.is_null());

  if (!on_buffers_available_cb_)
    return;

  // Note this should be done last since setBuffersAvailableListener() may
  // immediately invoke the callback if buffers came in during construction.
  Java_MediaCodecBridge_setBuffersAvailableListener(
      AttachCurrentThread(), j_bridge_, reinterpret_cast<intptr_t>(this));
}

MediaCodecBridgeImpl::~MediaCodecBridgeImpl() {
  JNIEnv* env = AttachCurrentThread();
  if (j_bridge_.obj())
    Java_MediaCodecBridge_release(env, j_bridge_);
}

void MediaCodecBridgeImpl::Stop() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_stop(env, j_bridge_);
}

MediaCodecResult MediaCodecBridgeImpl::Flush() {
  JNIEnv* env = AttachCurrentThread();
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_flush(env, j_bridge_));
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

MediaCodecResult MediaCodecBridgeImpl::GetOutputSize(gfx::Size* size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_bridge_);
  if (!result) {
    return {MediaCodecResult::Codes::kError, "Unable to get output format."};
  }

  size->SetSize(Java_MediaFormatWrapper_width(env, result),
                Java_MediaFormatWrapper_height(env, result));
  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::GetOutputSamplingRate(
    int* sampling_rate) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_bridge_);
  if (!result) {
    return {MediaCodecResult::Codes::kError, "Unable to get output format."};
  }

  *sampling_rate = Java_MediaFormatWrapper_sampleRate(env, result);
  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::GetOutputChannelCount(
    int* channel_count) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_bridge_);
  if (!result) {
    return {MediaCodecResult::Codes::kError, "Unable to get output format."};
  }

  *channel_count = Java_MediaFormatWrapper_channelCount(env, result);
  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::GetOutputColorSpace(
    gfx::ColorSpace* color_space) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_bridge_);
  if (!result) {
    return {MediaCodecResult::Codes::kError, "Unable to get output format."};
  }

  // TODO(liberato): Consider consolidating these to save JNI hops.  However,
  // since this is called only rarely, it's clearer this way.
  int standard = Java_MediaFormatWrapper_colorStandard(env, result);
  int range = Java_MediaFormatWrapper_colorRange(env, result);
  int transfer = Java_MediaFormatWrapper_colorTransfer(env, result);
  gfx::ColorSpace::PrimaryID primary_id;
  gfx::ColorSpace::TransferID transfer_id;
  gfx::ColorSpace::MatrixID matrix_id;
  gfx::ColorSpace::RangeID range_id;

  switch (standard) {
    case 1:  // MediaFormat.COLOR_STANDARD_BT709:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      matrix_id = gfx::ColorSpace::MatrixID::BT709;
      break;
    case 2:  // MediaFormat.COLOR_STANDARD_BT601_PAL:
      primary_id = gfx::ColorSpace::PrimaryID::BT470BG;
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case 4:  // MediaFormat.COLOR_STANDARD_BT601_NTSC:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTE170M;
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case 6:  // MediaFormat.COLOR_STANDARD_BT2020
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      matrix_id = gfx::ColorSpace::MatrixID::BT2020_NCL;
      break;
    default:
      DVLOG(3) << __func__ << ": unsupported primary in p: " << standard
               << " r: " << range << " t: " << transfer;
      return {MediaCodecResult::Codes::kError,
              "Unexpected MediaFormat.COLOR_STANDARD of " +
                  base::NumberToString(standard) + " specified."};
  }

  switch (transfer) {
    case 1:  // MediaFormat.COLOR_TRANSFER_LINEAR
      // TODO(liberato): LINEAR or LINEAR_HDR?
      // Based on https://android.googlesource.com/platform/frameworks/native/
      //            +/master/libs/nativewindow/include/android/data_space.h#57
      // we pick LINEAR_HDR.
      transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;
      break;
    case 3:  // MediaFormat.COLOR_TRANSFER_SDR_VIDEO
      transfer_id = gfx::ColorSpace::TransferID::SMPTE170M;
      break;
    case 6:  // MediaFormat.COLOR_TRANSFER_ST2084
      transfer_id = gfx::ColorSpace::TransferID::PQ;
      break;
    case 7:  // MediaFormat.COLOR_TRANSFER_HLG
      transfer_id = gfx::ColorSpace::TransferID::HLG;
      break;
    default:
      DVLOG(3) << __func__ << ": unsupported transfer in p: " << standard
               << " r: " << range << " t: " << transfer;
      return {MediaCodecResult::Codes::kError,
              "Unexpected MediaFormat.COLOR_TRANSFER of " +
                  base::NumberToString(transfer) + " specified."};
  }

  switch (range) {
    case 1:  // MediaFormat.COLOR_RANGE_FULL
      range_id = gfx::ColorSpace::RangeID::FULL;
      break;
    case 2:  // MediaFormat.COLOR_RANGE_LIMITED
      range_id = gfx::ColorSpace::RangeID::LIMITED;
      break;
    default:
      DVLOG(3) << __func__ << ": unsupported range in p: " << standard
               << " r: " << range << " t: " << transfer;
      return {MediaCodecResult::Codes::kError,
              "Unexpected MediaFormat.COLOR_RANGE of " +
                  base::NumberToString(range) + " specified."};
  }

  *color_space = gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);

  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::GetInputFormat(int* stride,
                                                      int* slice_height,
                                                      gfx::Size* encoded_size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getInputFormat(env, j_bridge_);
  if (!result) {
    return {MediaCodecResult::Codes::kError, "Failed to get input format."};
  }

  *stride = Java_MediaFormatWrapper_stride(env, result);
  *slice_height = Java_MediaFormatWrapper_yPlaneHeight(env, result);
  *encoded_size = gfx::Size(Java_MediaFormatWrapper_width(env, result),
                            Java_MediaFormatWrapper_height(env, result));
  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta presentation_time) {
  DVLOG(3) << __func__ << " " << index << ": " << data_size;
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return {MediaCodecResult::Codes::kError, "Input buffer size is too large."};
  }
  if (data && !FillInputBuffer(index, data, data_size)) {
    return {MediaCodecResult::Codes::kError, "Unable to fill input buffer."};
  }

  JNIEnv* env = AttachCurrentThread();
  MediaCodecStatus status =
      static_cast<MediaCodecStatus>(Java_MediaCodecBridge_queueInputBuffer(
          env, j_bridge_, index, 0, data_size,
          presentation_time.InMicroseconds(), 0));
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

MediaCodecResult MediaCodecBridgeImpl::QueueInputBlock(
    int index,
    base::span<const uint8_t> data,
    base::TimeDelta presentation_time,
    bool is_eos) {
  DVLOG(3) << __func__ << " " << index << ": " << data.size();
  if (data.size() >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return {MediaCodecResult::Codes::kError, "Input block size is too large."};
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_result =
      Java_MediaCodecBridge_obtainBlock(env, j_bridge_, data.size());
  ScopedJavaLocalRef<jobject> j_block =
      Java_ObtainBlockResult_block(env, j_result);
  ScopedJavaLocalRef<jobject> j_buffer =
      Java_ObtainBlockResult_buffer(env, j_result);
  if (j_buffer.is_null()) {
    Java_ObtainBlockResult_recycle(env, j_result);
    return {MediaCodecResult::Codes::kError, "Unable to obtain input block."};
  }

  if (!data.empty()) {
    base::android::JavaByteBufferToMutableSpan(env, j_buffer.obj())
        .copy_from_nonoverlapping(data);
  }

  MediaCodecStatus status =
      static_cast<MediaCodecStatus>(Java_MediaCodecBridge_queueInputBlock(
          env, j_bridge_, index, j_block, 0, data.size(),
          presentation_time.InMicroseconds(),
          is_eos ? kBufferFlagEndOfStream : 0));
  Java_ObtainBlockResult_recycle(env, j_result);
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

MediaCodecResult MediaCodecBridgeImpl::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    EncryptionScheme encryption_scheme,
    std::optional<EncryptionPattern> encryption_pattern,
    base::TimeDelta presentation_time) {
  DVLOG(3) << __func__ << " " << index << ": " << data_size;
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return {MediaCodecResult::Codes::kError, "Input buffer is too large."};
  }
  if (data && !FillInputBuffer(index, data, data_size)) {
    return {MediaCodecResult::Codes::kError, "Unable to fill input buffer."};
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_key_id = ToJavaByteArray(env, key_id);
  ScopedJavaLocalRef<jbyteArray> j_iv = ToJavaByteArray(env, iv);

  // The MediaCodec.CryptoInfo documentation says to pass NULL for |clear_array|
  // to indicate that all data is encrypted. But it doesn't specify what
  // |cypher_array| and |subsamples_size| should be in that case. We pass
  // one subsample here just to be on the safe side.
  int num_subsamples = std::max(static_cast<size_t>(1), subsamples.size());

  auto native_clear_array = base::HeapArray<jint>::Uninit(num_subsamples);
  auto native_cypher_array = base::HeapArray<jint>::Uninit(num_subsamples);

  if (subsamples.empty()) {
    native_clear_array[0] = 0;
    native_cypher_array[0] = data_size;
  } else {
    for (size_t i = 0; i < subsamples.size(); ++i) {
      DCHECK(subsamples[i].clear_bytes <= std::numeric_limits<uint16_t>::max());
      if (subsamples[i].cypher_bytes >
          static_cast<uint32_t>(std::numeric_limits<jint>::max())) {
        return {MediaCodecResult::Codes::kError,
                "Subsample size is too large."};
      }

      native_clear_array[i] = subsamples[i].clear_bytes;
      native_cypher_array[i] = subsamples[i].cypher_bytes;
    }
  }

  ScopedJavaLocalRef<jintArray> clear_array =
      base::android::ToJavaIntArray(env, native_clear_array);
  ScopedJavaLocalRef<jintArray> cypher_array =
      base::android::ToJavaIntArray(env, native_cypher_array);

  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_queueSecureInputBuffer(
          env, j_bridge_, index, 0, j_iv, j_key_id, clear_array, cypher_array,
          num_subsamples, static_cast<int>(encryption_scheme),
          static_cast<int>(
              encryption_pattern ? encryption_pattern->crypt_byte_block() : 0),
          static_cast<int>(
              encryption_pattern ? encryption_pattern->skip_byte_block() : 0),
          presentation_time.InMicroseconds()));
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

void MediaCodecBridgeImpl::QueueEOS(int input_buffer_index) {
  DVLOG(3) << __func__ << ": " << input_buffer_index;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_queueInputBuffer(env, j_bridge_, input_buffer_index, 0,
                                         0, 0, kBufferFlagEndOfStream);
}

MediaCodecResult MediaCodecBridgeImpl::DequeueInputBuffer(
    base::TimeDelta timeout,
    int* index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result = Java_MediaCodecBridge_dequeueInputBuffer(
      env, j_bridge_, timeout.InMicroseconds());
  *index = Java_DequeueInputResult_index(env, result);
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_DequeueInputResult_status(env, result));
  DVLOG(3) << __func__ << ": status: " << status << ", index: " << *index;
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

MediaCodecResult MediaCodecBridgeImpl::DequeueOutputBuffer(
    base::TimeDelta timeout,
    int* index,
    size_t* offset,
    size_t* size,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    bool* key_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_dequeueOutputBuffer(env, j_bridge_,
                                                timeout.InMicroseconds());
  *index = Java_DequeueOutputResult_index(env, result);
  *offset =
      base::checked_cast<size_t>(Java_DequeueOutputResult_offset(env, result));
  *size = base::checked_cast<size_t>(
      Java_DequeueOutputResult_numBytes(env, result));
  if (presentation_time) {
    *presentation_time = base::Microseconds(
        Java_DequeueOutputResult_presentationTimeMicroseconds(env, result));
  }
  int flags = Java_DequeueOutputResult_flags(env, result);
  if (end_of_stream)
    *end_of_stream = flags & kBufferFlagEndOfStream;
  if (key_frame)
    *key_frame = flags & kBufferFlagSyncFrame;
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_DequeueOutputResult_status(env, result));
  DVLOG(3) << __func__ << ": status: " << status << ", index: " << *index
           << ", offset: " << *offset << ", size: " << *size
           << ", flags: " << flags;
  return {ConvertToMediaCodecEnum(status), ApplyDescriptiveMessage(status)};
}

void MediaCodecBridgeImpl::ReleaseOutputBuffer(int index, bool render) {
  DVLOG(3) << __func__ << ": " << index;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_releaseOutputBuffer(env, j_bridge_, index, render);
}

MediaCodecResult MediaCodecBridgeImpl::GetInputBuffer(int input_buffer_index,
                                                      uint8_t** data,
                                                      size_t* capacity) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(
      Java_MediaCodecBridge_getInputBuffer(env, j_bridge_, input_buffer_index));
  if (j_buffer.is_null()) {
    return {MediaCodecResult::Codes::kError, "Unable to obtain input buffer."};
  }

  *data = static_cast<uint8_t*>(env->GetDirectBufferAddress(j_buffer.obj()));
  *capacity =
      base::checked_cast<size_t>(env->GetDirectBufferCapacity(j_buffer.obj()));
  return OkStatus();
}

MediaCodecResult MediaCodecBridgeImpl::CopyFromOutputBuffer(int index,
                                                            size_t offset,
                                                            void* dst,
                                                            size_t num) {
  const uint8_t* src_data = nullptr;
  size_t src_capacity = 0;
  MediaCodecResult result =
      GetOutputBufferAddress(index, offset, &src_data, &src_capacity);
  if (result.is_ok()) {
    CHECK_GE(src_capacity, num);
    memcpy(dst, src_data, num);
  }
  return result;
}

MediaCodecResult MediaCodecBridgeImpl::GetOutputBufferAddress(
    int index,
    size_t offset,
    const uint8_t** addr,
    size_t* capacity) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(
      Java_MediaCodecBridge_getOutputBuffer(env, j_bridge_, index));
  if (j_buffer.is_null()) {
    return {MediaCodecResult::Codes::kError, "Unable to get output buffer."};
  }
  const size_t total_capacity = env->GetDirectBufferCapacity(j_buffer.obj());
  CHECK_GE(total_capacity, offset);
  *addr = reinterpret_cast<const uint8_t*>(
              env->GetDirectBufferAddress(j_buffer.obj())) +
          offset;
  *capacity = total_capacity - offset;
  return OkStatus();
}

void MediaCodecBridgeImpl::OnBuffersAvailable(
    JNIEnv* /* env */,
    const base::android::JavaParamRef<jobject>& /* obj */) {
  on_buffers_available_cb_.Run();
}

std::string MediaCodecBridgeImpl::GetName() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_name =
      Java_MediaCodecBridge_getName(env, j_bridge_);
  return ConvertJavaStringToUTF8(env, j_name);
}

bool MediaCodecBridgeImpl::IsSoftwareCodec() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_isSoftwareCodec(env, j_bridge_);
}

bool MediaCodecBridgeImpl::SetSurface(const JavaRef<jobject>& surface) {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_setSurface(env, j_bridge_, surface);
}

void MediaCodecBridgeImpl::SetVideoBitrate(int bps, int frame_rate) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setVideoBitrate(env, j_bridge_, bps, frame_rate);
}

void MediaCodecBridgeImpl::RequestKeyFrameSoon() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_requestKeyFrameSoon(env, j_bridge_);
}

CodecType MediaCodecBridgeImpl::GetCodecType() const {
  return codec_type_;
}

size_t MediaCodecBridgeImpl::GetMaxInputSize() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getMaxInputSize(env, j_bridge_);
}

bool MediaCodecBridgeImpl::FillInputBuffer(int index,
                                           const uint8_t* data,
                                           size_t size) {
  uint8_t* dst = nullptr;
  size_t capacity = 0;
  if (!GetInputBuffer(index, &dst, &capacity).is_ok()) {
    LOG(ERROR) << "GetInputBuffer failed";
    return false;
  }
  CHECK(dst);

  if (size > capacity) {
    LOG(ERROR) << "Input buffer size " << size
               << " exceeds MediaCodec input buffer capacity: " << capacity;
    return false;
  }

  memcpy(dst, data, size);
  return true;
}

}  // namespace media
