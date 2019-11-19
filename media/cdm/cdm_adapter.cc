// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter.h"

#include <stddef.h>
#include <iomanip>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_initialized_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/key_systems.h"
#include "media/base/limits.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cdm/cdm_auxiliary_helper.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_type_conversion.h"
#include "media/cdm/cdm_wrapper.h"
#include "media/media_buildflags.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace media {

namespace {

// Constants for UMA reporting of file size (in KB) via
// UMA_HISTOGRAM_CUSTOM_COUNTS. Note that the histogram is log-scaled (rather
// than linear).
constexpr int kSizeKBMin = 1;
constexpr int kSizeKBMax = 512 * 1024;  // 512MB
constexpr int kSizeKBBuckets = 100;

// Only support version 1 of Storage Id. However, the "latest" version can also
// be requested.
constexpr uint32_t kRequestLatestStorageIdVersion = 0;
constexpr uint32_t kCurrentStorageIdVersion = 1;
static_assert(kCurrentStorageIdVersion < 0x80000000,
              "Versions 0x80000000 and above are reserved.");

// Verify that OutputProtection types matches those in CDM interface.
// Cannot use conversion function because these are used in bit masks.
// See CdmAdapter::EnableOutputProtection and
// CdmAdapter::OnQueryOutputProtectionStatusDone() below.
#define ASSERT_ENUM_EQ(media_enum, cdm_enum)                              \
  static_assert(                                                          \
      static_cast<int32_t>(media_enum) == static_cast<int32_t>(cdm_enum), \
      "Mismatched enum: " #media_enum " != " #cdm_enum)

ASSERT_ENUM_EQ(OutputProtection::LinkTypes::NONE, cdm::kLinkTypeNone);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::UNKNOWN, cdm::kLinkTypeUnknown);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::INTERNAL, cdm::kLinkTypeInternal);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::VGA, cdm::kLinkTypeVGA);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::HDMI, cdm::kLinkTypeHDMI);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::DVI, cdm::kLinkTypeDVI);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::DISPLAYPORT,
               cdm::kLinkTypeDisplayPort);
ASSERT_ENUM_EQ(OutputProtection::LinkTypes::NETWORK, cdm::kLinkTypeNetwork);
ASSERT_ENUM_EQ(OutputProtection::ProtectionType::NONE, cdm::kProtectionNone);
ASSERT_ENUM_EQ(OutputProtection::ProtectionType::HDCP, cdm::kProtectionHDCP);

std::string CdmStatusToString(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return "kSuccess";
    case cdm::kNoKey:
      return "kNoKey";
    case cdm::kNeedMoreData:
      return "kNeedMoreData";
    case cdm::kDecryptError:
      return "kDecryptError";
    case cdm::kDecodeError:
      return "kDecodeError";
    case cdm::kInitializationError:
      return "kInitializationError";
    case cdm::kDeferredInitialization:
      return "kDeferredInitialization";
  }

  NOTREACHED();
  return "Invalid Status!";
}

inline std::ostream& operator<<(std::ostream& out, cdm::Status status) {
  return out << CdmStatusToString(status);
}

std::string GetHexKeyId(const cdm::InputBuffer_2& buffer) {
  if (buffer.key_id_size == 0)
    return "N/A";

  return base::HexEncode(buffer.key_id, buffer.key_id_size);
}

std::string GetHexMask(uint32_t mask) {
  std::stringstream hex_string;
  hex_string << "0x" << std::setfill('0') << std::setw(8) << std::hex << mask;
  return hex_string.str();
}

void* GetCdmHost(int host_interface_version, void* user_data) {
  if (!host_interface_version || !user_data)
    return nullptr;

  static_assert(
      CheckSupportedCdmHostVersions(cdm::Host_10::kVersion,
                                    cdm::Host_11::kVersion),
      "Mismatch between GetCdmHost() and IsSupportedCdmHostVersion()");

  DCHECK(IsSupportedCdmHostVersion(host_interface_version));

  CdmAdapter* cdm_adapter = static_cast<CdmAdapter*>(user_data);
  DVLOG(1) << "Create CDM Host with version " << host_interface_version;
  switch (host_interface_version) {
    case cdm::Host_10::kVersion:
      return static_cast<cdm::Host_10*>(cdm_adapter);
    case cdm::Host_11::kVersion:
      return static_cast<cdm::Host_11*>(cdm_adapter);
    default:
      NOTREACHED() << "Unexpected host interface version "
                   << host_interface_version;
      return nullptr;
  }
}

void ReportSystemCodeUMA(const std::string& key_system, uint32_t system_code) {
  base::UmaHistogramSparse(
      "Media.EME." + GetKeySystemNameForUMA(key_system) + ".SystemCode",
      system_code);
}

// These are reported to UMA server. Do not renumber or reuse values.
enum OutputProtectionStatus {
  kQueried = 0,
  kNoExternalLink = 1,
  kAllExternalLinksProtected = 2,
  // Note: Only add new values immediately before this line.
  kStatusCount
};

void ReportOutputProtectionUMA(OutputProtectionStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Media.EME.OutputProtection", status,
                            OutputProtectionStatus::kStatusCount);
}

crash_reporter::CrashKeyString<256> g_origin_crash_key("cdm-origin");
using crash_reporter::ScopedCrashKeyString;

}  // namespace

// static
void CdmAdapter::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    CreateCdmFunc create_cdm_func,
    std::unique_ptr<CdmAuxiliaryHelper> helper,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DCHECK(!key_system.empty());
  DCHECK(session_message_cb);
  DCHECK(session_closed_cb);
  DCHECK(session_keys_change_cb);
  DCHECK(session_expiration_update_cb);

  scoped_refptr<CdmAdapter> cdm =
      new CdmAdapter(key_system, security_origin, cdm_config, create_cdm_func,
                     std::move(helper), session_message_cb, session_closed_cb,
                     session_keys_change_cb, session_expiration_update_cb);

  // |cdm| ownership passed to the promise.
  cdm->Initialize(std::make_unique<CdmInitializedPromise>(cdm_created_cb, cdm));
}

CdmAdapter::CdmAdapter(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    CreateCdmFunc create_cdm_func,
    std::unique_ptr<CdmAuxiliaryHelper> helper,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : key_system_(key_system),
      origin_string_(security_origin.Serialize()),
      cdm_config_(cdm_config),
      create_cdm_func_(create_cdm_func),
      helper_(std::move(helper)),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      pool_(new AudioBufferMemoryPool()) {
  DVLOG(1) << __func__;

  DCHECK(!key_system_.empty());
  DCHECK(create_cdm_func_);
  DCHECK(helper_);
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);

  helper_->SetFileReadCB(
      base::Bind(&CdmAdapter::OnFileRead, weak_factory_.GetWeakPtr()));
}

CdmAdapter::~CdmAdapter() {
  DVLOG(1) << __func__;

  // Reject any outstanding promises and close all the existing sessions.
  cdm_promise_adapter_.Clear();

  if (audio_init_cb_)
    audio_init_cb_.Run(false);
  if (video_init_cb_)
    video_init_cb_.Run(false);
}

CdmWrapper* CdmAdapter::CreateCdmInstance(const std::string& key_system) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "CdmAdapter::CreateCdmInstance");

  CdmWrapper* cdm = CdmWrapper::Create(create_cdm_func_, key_system.data(),
                                       key_system.size(), GetCdmHost, this);
  DVLOG(1) << "CDM instance for " + key_system + (cdm ? "" : " could not be") +
                  " created.";

  if (cdm) {
    // The interface version is relatively small. So using normal histogram
    // instead of a sparse histogram is okay. The following DCHECK asserts this.
    DCHECK(cdm->GetInterfaceVersion() <= 30);
    UMA_HISTOGRAM_ENUMERATION("Media.EME.CdmInterfaceVersion",
                              cdm->GetInterfaceVersion(), 30);
  }

  return cdm;
}

void CdmAdapter::Initialize(std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "CdmAdapter::Initialize");

  cdm_.reset(CreateCdmInstance(key_system_));
  if (!cdm_) {
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "Unable to create CDM.");
    return;
  }

  init_promise_id_ = cdm_promise_adapter_.SavePromise(std::move(promise));

  if (!cdm_->Initialize(cdm_config_.allow_distinctive_identifier,
                        cdm_config_.allow_persistent_state,
                        cdm_config_.use_hw_secure_codecs)) {
    // OnInitialized() will not be called by the CDM, which is the case for
    // CDM interfaces prior to CDM_10.
    OnInitialized(true);
    return;
  }

  // OnInitialized() will be called by the CDM.
}

int CdmAdapter::GetInterfaceVersion() {
  return cdm_->GetInterfaceVersion();
}

void CdmAdapter::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "CdmAdapter::SetServerCertificate");

  if (certificate.size() < limits::kMinCertificateLength ||
      certificate.size() > limits::kMaxCertificateLength) {
    promise->reject(CdmPromise::Exception::TYPE_ERROR, 0,
                    "Incorrect certificate.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->SetServerCertificate(promise_id, certificate.data(),
                             certificate.size());
}

void CdmAdapter::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "CdmAdapter::GetStatusForPolicy");

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": promise_id = " << promise_id;
  if (!cdm_->GetStatusForPolicy(promise_id,
                                ToCdmHdcpVersion(min_hdcp_version))) {
    DVLOG(1) << __func__ << ": GetStatusForPolicy not supported";
    cdm_promise_adapter_.RejectPromise(
        promise_id, CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
        "GetStatusForPolicy not supported.");
  }
}

void CdmAdapter::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "CdmAdapter::CreateSessionAndGenerateRequest");

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": promise_id = " << promise_id;

  cdm_->CreateSessionAndGenerateRequest(
      promise_id, ToCdmSessionType(session_type),
      ToCdmInitDataType(init_data_type), init_data.data(), init_data.size());
}

void CdmAdapter::LoadSession(CdmSessionType session_type,
                             const std::string& session_id,
                             std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::LoadSession", "session_id", session_id);

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": session_id = " << session_id
           << ", promise_id = " << promise_id;

  cdm_->LoadSession(promise_id, ToCdmSessionType(session_type),
                    session_id.data(), session_id.size());
}

void CdmAdapter::UpdateSession(const std::string& session_id,
                               const std::vector<uint8_t>& response,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());
  DCHECK(!response.empty());
  TRACE_EVENT1("media", "CdmAdapter::UpdateSession", "session_id", session_id);

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": session_id = " << session_id
           << ", promise_id = " << promise_id;

  cdm_->UpdateSession(promise_id, session_id.data(), session_id.size(),
                      response.data(), response.size());
}

void CdmAdapter::CloseSession(const std::string& session_id,
                              std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());
  TRACE_EVENT1("media", "CdmAdapter::CloseSession", "session_id", session_id);

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": session_id = " << session_id
           << ", promise_id = " << promise_id;

  cdm_->CloseSession(promise_id, session_id.data(), session_id.size());
}

void CdmAdapter::RemoveSession(const std::string& session_id,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());
  TRACE_EVENT1("media", "CdmAdapter::RemoveSession", "session_id", session_id);

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  DVLOG(2) << __func__ << ": session_id = " << session_id
           << ", promise_id = " << promise_id;

  cdm_->RemoveSession(promise_id, session_id.data(), session_id.size());
}

CdmContext* CdmAdapter::GetCdmContext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return this;
}

std::unique_ptr<CallbackRegistration> CdmAdapter::RegisterEventCB(
    EventCB event_cb) {
  NOTIMPLEMENTED();
  return nullptr;
}

Decryptor* CdmAdapter::GetDecryptor() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // When using HW secure codecs, we cannot and should not use the CDM instance
  // to do decrypt and/or decode. Instead, we should use the CdmProxy.
  // TODO(xhwang): Fix External Clear Key key system to be able to set
  // |use_hw_secure_codecs| so that we don't have to check both.
  // TODO(xhwang): Update this logic to support transcryption.
  if (cdm_config_.use_hw_secure_codecs || cdm_proxy_created_) {
    DVLOG(2) << __func__ << ": GetDecryptor() returns null";
    return nullptr;
  }

  return this;
}

int CdmAdapter::GetCdmId() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
#if BUILDFLAG(ENABLE_CDM_PROXY)
  int cdm_id = helper_->GetCdmProxyCdmId();
  DVLOG(2) << __func__ << ": cdm_id = " << cdm_id;
  return cdm_id;
#else
  return CdmContext::kInvalidCdmId;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)
}

void CdmAdapter::RegisterNewKeyCB(StreamType stream_type,
                                  const NewKeyCB& key_added_cb) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = key_added_cb;
      return;
    case kVideo:
      new_video_key_cb_ = key_added_cb;
      return;
  }

  NOTREACHED() << "Unexpected StreamType " << stream_type;
}

void CdmAdapter::Decrypt(StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         const DecryptCB& decrypt_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  ScopedCrashKeyString scoped_crash_key(&g_origin_crash_key, origin_string_);

  cdm::InputBuffer_2 input_buffer = {};
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<DecryptedBlockImpl> decrypted_block(new DecryptedBlockImpl());

  TRACE_EVENT_BEGIN1("media", "CdmAdapter::Decrypt", "stream_type",
                     stream_type);
  ToCdmInputBuffer(*encrypted, &subsamples, &input_buffer);
  cdm::Status status = cdm_->Decrypt(input_buffer, decrypted_block.get());
  TRACE_EVENT_END2("media", "CdmAdapter::Decrypt", "key ID",
                   GetHexKeyId(input_buffer), "status",
                   CdmStatusToString(status));

  if (status != cdm::kSuccess) {
    DVLOG(1) << __func__ << ": status = " << status;
    decrypt_cb.Run(ToMediaDecryptorStatus(status), nullptr);
    return;
  }

  scoped_refptr<DecoderBuffer> decrypted_buffer(
      DecoderBuffer::CopyFrom(decrypted_block->DecryptedBuffer()->Data(),
                              decrypted_block->DecryptedBuffer()->Size()));
  decrypted_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(decrypted_block->Timestamp()));
  decrypt_cb.Run(Decryptor::kSuccess, std::move(decrypted_buffer));
}

void CdmAdapter::CancelDecrypt(StreamType stream_type) {
  // As the Decrypt methods are synchronous, nothing can be done here.
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void CdmAdapter::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                        const DecoderInitCB& init_cb) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!audio_init_cb_);
  TRACE_EVENT0("media", "CdmAdapter::InitializeAudioDecode");

  auto cdm_config = ToCdmAudioDecoderConfig(config);
  if (cdm_config.codec == cdm::kUnknownAudioCodec) {
    DVLOG(1) << __func__
             << ": Unsupported config: " << config.AsHumanReadableString();
    init_cb.Run(false);
    return;
  }

  cdm::Status status = cdm_->InitializeAudioDecoder(cdm_config);
  if (status != cdm::kSuccess && status != cdm::kDeferredInitialization) {
    DCHECK(status == cdm::kInitializationError);
    DVLOG(1) << __func__ << ": status = " << status;
    init_cb.Run(false);
    return;
  }

  audio_samples_per_second_ = config.samples_per_second();
  audio_channel_layout_ = config.channel_layout();

  if (status == cdm::kDeferredInitialization) {
    DVLOG(1) << "Deferred initialization in " << __func__;
    audio_init_cb_ = init_cb;
    return;
  }

  init_cb.Run(true);
}

void CdmAdapter::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                        const DecoderInitCB& init_cb) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!video_init_cb_);
  TRACE_EVENT0("media", "CdmAdapter::InitializeVideoDecoder");

  // Alpha decoding is not supported by the CDM.
  if (config.alpha_mode() != VideoDecoderConfig::AlphaMode::kIsOpaque) {
    DVLOG(1) << __func__
             << ": Unsupported config: " << config.AsHumanReadableString();
    init_cb.Run(false);
    return;
  }

  // cdm::kUnknownVideoCodecProfile and cdm::kUnknownVideoFormat are not checked
  // because it's possible the container has wrong information or the demuxer
  // doesn't parse them correctly.
  auto cdm_config = ToCdmVideoDecoderConfig(config);
  if (cdm_config.codec == cdm::kUnknownVideoCodec) {
    DVLOG(1) << __func__
             << ": Unsupported config: " << config.AsHumanReadableString();
    init_cb.Run(false);
    return;
  }

  cdm::Status status = cdm_->InitializeVideoDecoder(cdm_config);
  if (status != cdm::kSuccess && status != cdm::kDeferredInitialization) {
    DCHECK(status == cdm::kInitializationError);
    DVLOG(1) << __func__ << ": status = " << status;
    init_cb.Run(false);
    return;
  }

  pixel_aspect_ratio_ = config.GetPixelAspectRatio();
  is_video_encrypted_ = config.is_encrypted();

  if (status == cdm::kDeferredInitialization) {
    DVLOG(1) << "Deferred initialization in " << __func__;
    video_init_cb_ = init_cb;
    return;
  }

  init_cb.Run(true);
}

void CdmAdapter::DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                                       const AudioDecodeCB& audio_decode_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  ScopedCrashKeyString scoped_crash_key(&g_origin_crash_key, origin_string_);

  cdm::InputBuffer_2 input_buffer = {};
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<AudioFramesImpl> audio_frames(new AudioFramesImpl());

  TRACE_EVENT_BEGIN0("media", "CdmAdapter::DecryptAndDecodeAudio");
  ToCdmInputBuffer(*encrypted, &subsamples, &input_buffer);
  cdm::Status status =
      cdm_->DecryptAndDecodeSamples(input_buffer, audio_frames.get());
  TRACE_EVENT_END2("media", "CdmAdapter::DecryptAndDecodeAudio", "key ID",
                   GetHexKeyId(input_buffer), "status",
                   CdmStatusToString(status));

  const Decryptor::AudioFrames empty_frames;
  if (status != cdm::kSuccess) {
    DVLOG(1) << __func__ << ": status = " << status;
    audio_decode_cb.Run(ToMediaDecryptorStatus(status), empty_frames);
    return;
  }

  Decryptor::AudioFrames audio_frame_list;
  DCHECK(audio_frames->FrameBuffer());
  if (!AudioFramesDataToAudioFrames(std::move(audio_frames),
                                    &audio_frame_list)) {
    DVLOG(1) << __func__ << " unable to convert Audio Frames";
    audio_decode_cb.Run(Decryptor::kError, empty_frames);
    return;
  }

  audio_decode_cb.Run(Decryptor::kSuccess, audio_frame_list);
}

void CdmAdapter::DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                                       const VideoDecodeCB& video_decode_cb) {
  DVLOG(3) << __func__ << ": " << encrypted->AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  ScopedCrashKeyString scoped_crash_key(&g_origin_crash_key, origin_string_);

  cdm::InputBuffer_2 input_buffer = {};
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<VideoFrameImpl> video_frame = helper_->CreateCdmVideoFrame();

  TRACE_EVENT_BEGIN1(
      "media", "CdmAdapter::DecryptAndDecodeVideo", "buffer type",
      encrypted->end_of_stream()
          ? "end of stream"
          : (encrypted->is_key_frame() ? "key frame" : "non-key frame"));
  ToCdmInputBuffer(*encrypted, &subsamples, &input_buffer);
  cdm::Status status =
      cdm_->DecryptAndDecodeFrame(input_buffer, video_frame.get());
  TRACE_EVENT_END2("media", "CdmAdapter::DecryptAndDecodeVideo", "key ID",
                   GetHexKeyId(input_buffer), "status",
                   CdmStatusToString(status));

  if (status != cdm::kSuccess) {
    DVLOG(1) << __func__ << ": status = " << status;
    video_decode_cb.Run(ToMediaDecryptorStatus(status), nullptr);
    return;
  }

  gfx::Rect visible_rect(video_frame->Size().width, video_frame->Size().height);
  scoped_refptr<VideoFrame> decoded_frame = video_frame->TransformToVideoFrame(
      GetNaturalSize(visible_rect, pixel_aspect_ratio_));
  if (!decoded_frame) {
    DLOG(ERROR) << __func__ << ": TransformToVideoFrame failed.";
    video_decode_cb.Run(Decryptor::kError, nullptr);
    return;
  }

  if (is_video_encrypted_) {
    decoded_frame->metadata()->SetBoolean(VideoFrameMetadata::PROTECTED_VIDEO,
                                          true);
  }

  video_decode_cb.Run(Decryptor::kSuccess, decoded_frame);
}

void CdmAdapter::ResetDecoder(StreamType stream_type) {
  DVLOG(2) << __func__ << ": stream_type = " << stream_type;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::ResetDecoder", "stream_type", stream_type);

  cdm_->ResetDecoder(ToCdmStreamType(stream_type));
}

void CdmAdapter::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(2) << __func__ << ": stream_type = " << stream_type;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::DeinitializeDecoder", "stream_type",
               stream_type);

  cdm_->DeinitializeDecoder(ToCdmStreamType(stream_type));

  // Reset the saved values from initializing the decoder.
  switch (stream_type) {
    case Decryptor::kAudio:
      audio_samples_per_second_ = 0;
      audio_channel_layout_ = CHANNEL_LAYOUT_NONE;
      break;
    case Decryptor::kVideo:
      pixel_aspect_ratio_ = 0.0;
      break;
  }
}

cdm::Buffer* CdmAdapter::Allocate(uint32_t capacity) {
  DVLOG(3) << __func__ << ": capacity = " << capacity;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::Allocate", "capacity", capacity);

  return helper_->CreateCdmBuffer(capacity);
}

void CdmAdapter::SetTimer(int64_t delay_ms, void* context) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto delay = base::TimeDelta::FromMilliseconds(delay_ms);
  DVLOG(3) << __func__ << ": delay = " << delay << ", context = " << context;
  TRACE_EVENT2("media", "CdmAdapter::SetTimer", "delay_ms", delay_ms, "context",
               context);

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CdmAdapter::TimerExpired, weak_factory_.GetWeakPtr(),
                     context),
      delay);
}

void CdmAdapter::TimerExpired(void* context) {
  DVLOG(3) << __func__ << ": context = " << context;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::TimerExpired", "context", context);

  cdm_->TimerExpired(context);
}

cdm::Time CdmAdapter::GetCurrentWallTime() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return base::Time::Now().ToDoubleT();
}

void CdmAdapter::OnInitialized(bool success) {
  DVLOG(3) << __func__ << ": success = " << success;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(init_promise_id_, CdmPromiseAdapter::kInvalidPromiseId);

  if (!success) {
    cdm_promise_adapter_.RejectPromise(
        init_promise_id_, CdmPromise::Exception::INVALID_STATE_ERROR, 0,
        "Unable to create CDM.");
  } else {
    cdm_promise_adapter_.ResolvePromise(init_promise_id_);
  }

  init_promise_id_ = CdmPromiseAdapter::kInvalidPromiseId;
}

void CdmAdapter::OnResolveKeyStatusPromise(uint32_t promise_id,
                                           cdm::KeyStatus key_status) {
  DVLOG(2) << __func__ << ": promise_id = " << promise_id
           << ", key_status = " << key_status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.ResolvePromise(promise_id, ToMediaKeyStatus(key_status));
}

void CdmAdapter::OnResolvePromise(uint32_t promise_id) {
  DVLOG(2) << __func__ << ": promise_id = " << promise_id;
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void CdmAdapter::OnResolveNewSessionPromise(uint32_t promise_id,
                                            const char* session_id,
                                            uint32_t session_id_size) {
  DVLOG(2) << __func__ << ": promise_id = " << promise_id;
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.ResolvePromise(promise_id,
                                      std::string(session_id, session_id_size));
}

void CdmAdapter::OnRejectPromise(uint32_t promise_id,
                                 cdm::Exception exception,
                                 uint32_t system_code,
                                 const char* error_message,
                                 uint32_t error_message_size) {
  std::string error_message_str(error_message, error_message_size);
  DVLOG(2) << __func__ << ": promise_id = " << promise_id
           << ", exception = " << exception << ", system_code = " << system_code
           << ", error_message = " << error_message_str;

  // This is the central place for library CDM promise rejection. Cannot report
  // this in more generic classes like CdmPromise or CdmPromiseAdapter because
  // they may be used multiple times in one promise chain that involves IPC.
  ReportSystemCodeUMA(key_system_, system_code);

  // UMA to help track file related errors. See http://crbug.com/410630
  if (system_code == 0x27) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.EME.CdmFileIO.FileSizeKBOnError",
                                last_read_file_size_kb_, kSizeKBMin, kSizeKBMax,
                                kSizeKBBuckets);
  }

  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.RejectPromise(promise_id,
                                     ToMediaCdmPromiseException(exception),
                                     system_code, error_message_str);
}

void CdmAdapter::OnSessionMessage(const char* session_id,
                                  uint32_t session_id_size,
                                  cdm::MessageType message_type,
                                  const char* message,
                                  uint32_t message_size) {
  std::string session_id_str(session_id, session_id_size);
  DVLOG(2) << __func__ << ": session_id = " << session_id_str;
  DCHECK(task_runner_->BelongsToCurrentThread());

  TRACE_EVENT2("media", "CdmAdapter::OnSessionMessage", "session_id",
               session_id_str, "message_type", message_type);

  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(message);
  session_message_cb_.Run(
      session_id_str, ToMediaMessageType(message_type),
      std::vector<uint8_t>(message_ptr, message_ptr + message_size));
}

void CdmAdapter::OnSessionKeysChange(const char* session_id,
                                     uint32_t session_id_size,
                                     bool has_additional_usable_key,
                                     const cdm::KeyInformation* keys_info,
                                     uint32_t keys_info_count) {
  std::string session_id_str(session_id, session_id_size);
  DVLOG(2) << __func__ << ": session_id = " << session_id_str;
  DCHECK(task_runner_->BelongsToCurrentThread());

  TRACE_EVENT2("media", "CdmAdapter::OnSessionKeysChange", "session_id",
               session_id_str, "has_additional_usable_key",
               has_additional_usable_key);

  CdmKeysInfo keys;
  keys.reserve(keys_info_count);
  for (uint32_t i = 0; i < keys_info_count; ++i) {
    const auto& info = keys_info[i];
    keys.push_back(std::make_unique<CdmKeyInformation>(
        info.key_id, info.key_id_size, ToMediaKeyStatus(info.status),
        info.system_code));
  }

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key) {
    if (new_audio_key_cb_)
      new_audio_key_cb_.Run();
    if (new_video_key_cb_)
      new_video_key_cb_.Run();
  }

  session_keys_change_cb_.Run(session_id_str, has_additional_usable_key,
                              std::move(keys));
}

void CdmAdapter::OnExpirationChange(const char* session_id,
                                    uint32_t session_id_size,
                                    cdm::Time new_expiry_time) {
  std::string session_id_str(session_id, session_id_size);
  DVLOG(2) << __func__ << ": session_id = " << session_id_str
           << ", new_expiry_time = " << new_expiry_time;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::Time expiration = base::Time::FromDoubleT(new_expiry_time);
  TRACE_EVENT2("media", "CdmAdapter::OnExpirationChange", "session_id",
               session_id_str, "new_expiry_time", expiration);
  session_expiration_update_cb_.Run(session_id_str, expiration);
}

void CdmAdapter::OnSessionClosed(const char* session_id,
                                 uint32_t session_id_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::string session_id_str(session_id, session_id_size);
  TRACE_EVENT1("media", "CdmAdapter::OnSessionClosed", "session_id",
               session_id_str);
  session_closed_cb_.Run(session_id_str);
}

void CdmAdapter::SendPlatformChallenge(const char* service_id,
                                       uint32_t service_id_size,
                                       const char* challenge,
                                       uint32_t challenge_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!cdm_config_.allow_distinctive_identifier) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindRepeating(&CdmAdapter::OnChallengePlatformDone,
                            weak_factory_.GetWeakPtr(), false, "", "", ""));
    return;
  }

  helper_->ChallengePlatform(std::string(service_id, service_id_size),
                             std::string(challenge, challenge_size),
                             base::Bind(&CdmAdapter::OnChallengePlatformDone,
                                        weak_factory_.GetWeakPtr()));
}

void CdmAdapter::OnChallengePlatformDone(
    bool success,
    const std::string& signed_data,
    const std::string& signed_data_signature,
    const std::string& platform_key_certificate) {
  DVLOG(2) << __func__ << ": success = " << success;
  TRACE_EVENT1("media", "CdmAdapter::OnChallengePlatformDone", "success",
               success);

  cdm::PlatformChallengeResponse platform_challenge_response = {};
  if (success) {
    platform_challenge_response.signed_data =
        reinterpret_cast<const uint8_t*>(signed_data.data());
    platform_challenge_response.signed_data_length = signed_data.length();
    platform_challenge_response.signed_data_signature =
        reinterpret_cast<const uint8_t*>(signed_data_signature.data());
    platform_challenge_response.signed_data_signature_length =
        signed_data_signature.length();
    platform_challenge_response.platform_key_certificate =
        reinterpret_cast<const uint8_t*>(platform_key_certificate.data());
    platform_challenge_response.platform_key_certificate_length =
        platform_key_certificate.length();
  }

  cdm_->OnPlatformChallengeResponse(platform_challenge_response);
}

void CdmAdapter::EnableOutputProtection(uint32_t desired_protection_mask) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::EnableOutputProtection",
               "desired_protection_mask", GetHexMask(desired_protection_mask));

  helper_->EnableProtection(
      desired_protection_mask,
      base::BindOnce(&CdmAdapter::OnEnableOutputProtectionDone,
                     weak_factory_.GetWeakPtr()));
}

void CdmAdapter::OnEnableOutputProtectionDone(bool success) {
  // CDM needs to call QueryOutputProtectionStatus() to see if it took effect
  // or not.
  DVLOG(1) << __func__ << ": success = " << success;
  TRACE_EVENT1("media", "CdmAdapter::OnEnableOutputProtectionDone", "success",
               success);
}

void CdmAdapter::QueryOutputProtectionStatus() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "CdmAdapter::QueryOutputProtectionStatus");

  ReportOutputProtectionQuery();
  helper_->QueryStatus(
      base::Bind(&CdmAdapter::OnQueryOutputProtectionStatusDone,
                 weak_factory_.GetWeakPtr()));
}

void CdmAdapter::OnQueryOutputProtectionStatusDone(bool success,
                                                   uint32_t link_mask,
                                                   uint32_t protection_mask) {
  DVLOG(2) << __func__ << ": success = " << success;
  // Combining |link_mask| and |protection_mask| since there's no TRACE_EVENT3.
  TRACE_EVENT2("media", "CdmAdapter::OnQueryOutputProtectionStatusDone",
               "success", success, "link_mask, protection_mask",
               GetHexMask(link_mask) + ", " + GetHexMask(protection_mask));

  // The bit mask definition must be consistent between media::OutputProtection
  // and cdm::ContentDecryptionModule* interfaces. This is statically asserted
  // by ASSERT_ENUM_EQs above.

  // Return a query status of failure on error.
  cdm::QueryResult query_result;
  if (success) {
    query_result = cdm::kQuerySucceeded;
    ReportOutputProtectionQueryResult(link_mask, protection_mask);
  } else {
    DVLOG(1) << __func__ << ": query output protection status failed";
    query_result = cdm::kQueryFailed;
  }

  cdm_->OnQueryOutputProtectionStatus(query_result, link_mask, protection_mask);
}

void CdmAdapter::ReportOutputProtectionQuery() {
  if (uma_for_output_protection_query_reported_)
    return;

  ReportOutputProtectionUMA(OutputProtectionStatus::kQueried);
  uma_for_output_protection_query_reported_ = true;
}

void CdmAdapter::ReportOutputProtectionQueryResult(uint32_t link_mask,
                                                   uint32_t protection_mask) {
  DCHECK(uma_for_output_protection_query_reported_);

  if (uma_for_output_protection_positive_result_reported_)
    return;

  // Report UMAs for output protection query result.

  uint32_t external_links = (link_mask & ~cdm::kLinkTypeInternal);

  if (!external_links) {
    ReportOutputProtectionUMA(OutputProtectionStatus::kNoExternalLink);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  const uint32_t kProtectableLinks =
      cdm::kLinkTypeHDMI | cdm::kLinkTypeDVI | cdm::kLinkTypeDisplayPort;
  bool is_unprotectable_link_connected =
      (external_links & ~kProtectableLinks) != 0;
  bool is_hdcp_enabled_on_all_protectable_links =
      (protection_mask & cdm::kProtectionHDCP) != 0;

  if (!is_unprotectable_link_connected &&
      is_hdcp_enabled_on_all_protectable_links) {
    ReportOutputProtectionUMA(
        OutputProtectionStatus::kAllExternalLinksProtected);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  // Do not report a negative result because it could be a false negative.
  // Instead, we will calculate number of negatives using the total number of
  // queries and positive results.
}

void CdmAdapter::OnDeferredInitializationDone(cdm::StreamType stream_type,
                                              cdm::Status decoder_status) {
  DVLOG(1) << __func__ << ": stream_type = " << stream_type
           << ", decoder_status = " << decoder_status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (stream_type) {
    case cdm::kStreamTypeAudio:
      std::move(audio_init_cb_).Run(decoder_status == cdm::kSuccess);
      return;
    case cdm::kStreamTypeVideo:
      std::move(video_init_cb_).Run(decoder_status == cdm::kSuccess);
      return;
  }

  NOTREACHED() << "Unexpected cdm::StreamType " << stream_type;
}

cdm::FileIO* CdmAdapter::CreateFileIO(cdm::FileIOClient* client) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!cdm_config_.allow_persistent_state) {
    DVLOG(1) << __func__ << ": Persistent state not allowed.";
    return nullptr;
  }

  return helper_->CreateCdmFileIO(client);
}

void CdmAdapter::RequestStorageId(uint32_t version) {
  DVLOG(2) << __func__ << ": version = " << version;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!cdm_config_.allow_persistent_state ||
      !(version == kCurrentStorageIdVersion ||
        version == kRequestLatestStorageIdVersion)) {
    DVLOG(1) << __func__ << ": Persistent state not allowed ("
             << cdm_config_.allow_persistent_state
             << ") or invalid storage ID version (" << version << ").";
    task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(&CdmAdapter::OnStorageIdObtained,
                                       weak_factory_.GetWeakPtr(), version,
                                       std::vector<uint8_t>()));
    return;
  }

  helper_->GetStorageId(version, base::Bind(&CdmAdapter::OnStorageIdObtained,
                                            weak_factory_.GetWeakPtr()));
}

cdm::CdmProxy* CdmAdapter::RequestCdmProxy(cdm::CdmProxyClient* client) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // CdmProxy should only be created once, at CDM initialization time.
  if (cdm_proxy_created_ ||
      init_promise_id_ == CdmPromiseAdapter::kInvalidPromiseId) {
    DVLOG(1) << __func__
             << ": CdmProxy can only be created once, and must be created "
                "during CDM initialization.";
    return nullptr;
  }

  cdm_proxy_created_ = true;
  return helper_->CreateCdmProxy(client);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)
}

void CdmAdapter::OnStorageIdObtained(uint32_t version,
                                     const std::vector<uint8_t>& storage_id) {
  DVLOG(2) << __func__ << ": version = " << version;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "CdmAdapter::OnStorageIdObtained", "version", version);

  cdm_->OnStorageId(version, storage_id.data(), storage_id.size());
}

bool CdmAdapter::AudioFramesDataToAudioFrames(
    std::unique_ptr<AudioFramesImpl> audio_frames,
    Decryptor::AudioFrames* result_frames) {
  const uint8_t* data = audio_frames->FrameBuffer()->Data();
  const size_t data_size = audio_frames->FrameBuffer()->Size();
  size_t bytes_left = data_size;
  const SampleFormat sample_format =
      ToMediaSampleFormat(audio_frames->Format());
  const int audio_channel_count =
      ChannelLayoutToChannelCount(audio_channel_layout_);
  const int audio_bytes_per_frame =
      SampleFormatToBytesPerChannel(sample_format) * audio_channel_count;
  if (audio_bytes_per_frame <= 0)
    return false;

  // Allocate space for the channel pointers given to AudioBuffer.
  std::vector<const uint8_t*> channel_ptrs(audio_channel_count, nullptr);
  do {
    // AudioFrames can contain multiple audio output buffers, which are
    // serialized into this format:
    // |<------------------- serialized audio buffer ------------------->|
    // | int64_t timestamp | int64_t length | length bytes of audio data |
    int64_t timestamp = 0;
    int64_t frame_size = -1;
    const size_t kHeaderSize = sizeof(timestamp) + sizeof(frame_size);
    if (bytes_left < kHeaderSize)
      return false;

    memcpy(&timestamp, data, sizeof(timestamp));
    memcpy(&frame_size, data + sizeof(timestamp), sizeof(frame_size));
    data += kHeaderSize;
    bytes_left -= kHeaderSize;

    // We should *not* have empty frames in the list.
    if (frame_size <= 0 ||
        bytes_left < base::checked_cast<size_t>(frame_size)) {
      return false;
    }

    // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
    // one in the case of interleaved data.
    const int size_per_channel = frame_size / audio_channel_count;
    for (int i = 0; i < audio_channel_count; ++i)
      channel_ptrs[i] = data + i * size_per_channel;

    const int frame_count = frame_size / audio_bytes_per_frame;
    scoped_refptr<media::AudioBuffer> frame = media::AudioBuffer::CopyFrom(
        sample_format, audio_channel_layout_, audio_channel_count,
        audio_samples_per_second_, frame_count, &channel_ptrs[0],
        base::TimeDelta::FromMicroseconds(timestamp), pool_);
    result_frames->push_back(frame);

    data += frame_size;
    bytes_left -= frame_size;
  } while (bytes_left > 0);

  return true;
}

void CdmAdapter::OnFileRead(int file_size_bytes) {
  DCHECK_GE(file_size_bytes, 0);
  last_read_file_size_kb_ = file_size_bytes / 1024;

  if (file_size_uma_reported_)
    return;

  UMA_HISTOGRAM_CUSTOM_COUNTS("Media.EME.CdmFileIO.FileSizeKBOnFirstRead",
                              last_read_file_size_kb_, kSizeKBMin, kSizeKBMax,
                              kSizeKBBuckets);
  file_size_uma_reported_ = true;
}

}  // namespace media
