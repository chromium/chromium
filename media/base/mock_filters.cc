// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/check_op.h"
#include "media/base/demuxer.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

MATCHER(NotEmpty, "") {
  return !arg.empty();
}

namespace media {

MockPipelineClient::MockPipelineClient() = default;
MockPipelineClient::~MockPipelineClient() = default;

MockPipeline::MockPipeline() = default;
MockPipeline::~MockPipeline() = default;

MockMediaResource::MockMediaResource() = default;
MockMediaResource::~MockMediaResource() = default;

MockDemuxer::MockDemuxer() = default;
MockDemuxer::~MockDemuxer() = default;

std::string MockDemuxer::GetDisplayName() const {
  return "MockDemuxer";
}

DemuxerType MockDemuxer::GetDemuxerType() const {
  return DemuxerType::kMockDemuxer;
}

MockDemuxerStream::MockDemuxerStream(DemuxerStream::Type type) : type_(type) {}

MockDemuxerStream::~MockDemuxerStream() = default;

DemuxerStream::Type MockDemuxerStream::type() const {
  return type_;
}

StreamLiveness MockDemuxerStream::liveness() const {
  return liveness_;
}

AudioDecoderConfig MockDemuxerStream::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig MockDemuxerStream::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  return video_decoder_config_;
}

void MockDemuxerStream::set_audio_decoder_config(
    const AudioDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  audio_decoder_config_ = config;
}

void MockDemuxerStream::set_video_decoder_config(
    const VideoDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  video_decoder_config_ = config;
}

void MockDemuxerStream::set_liveness(StreamLiveness liveness) {
  liveness_ = liveness;
}

MockVideoDecoder::MockVideoDecoder() : MockVideoDecoder(0) {}

MockVideoDecoder::MockVideoDecoder(int decoder_id)
    : MockVideoDecoder(false, false, decoder_id) {}

MockVideoDecoder::MockVideoDecoder(bool is_platform_decoder,
                                   bool supports_decryption,
                                   int decoder_id)
    : is_platform_decoder_(is_platform_decoder),
      supports_decryption_(supports_decryption),
      decoder_id_(decoder_id) {
  ON_CALL(*this, CanReadWithoutStalling()).WillByDefault(Return(true));
}

MockVideoDecoder::~MockVideoDecoder() = default;

bool MockVideoDecoder::IsPlatformDecoder() const {
  return is_platform_decoder_;
}

bool MockVideoDecoder::SupportsDecryption() const {
  return supports_decryption_;
}

MockAudioEncoder::MockAudioEncoder() = default;
MockAudioEncoder::~MockAudioEncoder() {
  OnDestruct();
}

MockVideoEncoder::MockVideoEncoder() = default;
MockVideoEncoder::~MockVideoEncoder() {
  Dtor();
}

MockAudioDecoder::MockAudioDecoder() : MockAudioDecoder(0) {}

MockAudioDecoder::MockAudioDecoder(int decoder_id)
    : MockAudioDecoder(false, false, decoder_id) {}

MockAudioDecoder::MockAudioDecoder(bool is_platform_decoder,
                                   bool supports_decryption,
                                   int decoder_id)
    : is_platform_decoder_(is_platform_decoder),
      supports_decryption_(supports_decryption),
      decoder_id_(decoder_id) {}

MockAudioDecoder::~MockAudioDecoder() = default;

bool MockAudioDecoder::IsPlatformDecoder() const {
  return is_platform_decoder_;
}

bool MockAudioDecoder::SupportsDecryption() const {
  return supports_decryption_;
}

MockRendererClient::MockRendererClient() = default;

MockRendererClient::~MockRendererClient() = default;

MockVideoRenderer::MockVideoRenderer() = default;

MockVideoRenderer::~MockVideoRenderer() = default;

MockAudioRenderer::MockAudioRenderer() = default;

MockAudioRenderer::~MockAudioRenderer() = default;

MockRenderer::MockRenderer() = default;

MockRenderer::~MockRenderer() = default;

MockRendererFactory::MockRendererFactory() = default;

MockRendererFactory::~MockRendererFactory() = default;

MockTimeSource::MockTimeSource() = default;

MockTimeSource::~MockTimeSource() = default;

MockCdmClient::MockCdmClient() = default;

MockCdmClient::~MockCdmClient() = default;

MockDecryptor::MockDecryptor() = default;

MockDecryptor::~MockDecryptor() = default;

MockCdmContext::MockCdmContext() = default;

MockCdmContext::~MockCdmContext() = default;

std::optional<base::UnguessableToken> MockCdmContext::GetCdmId() const {
  return cdm_id_;
}

void MockCdmContext::set_cdm_id(const base::UnguessableToken& cdm_id) {
  cdm_id_ = std::make_optional(cdm_id);
}

MockCdmPromise::MockCdmPromise(bool expect_success) {
  if (expect_success) {
    EXPECT_CALL(*this, resolve());
    EXPECT_CALL(*this, reject(_, _, _)).Times(0);
  } else {
    EXPECT_CALL(*this, resolve()).Times(0);
    EXPECT_CALL(*this, reject(_, _, NotEmpty()));
  }
}

MockCdmPromise::~MockCdmPromise() {
  // The EXPECT calls will verify that the promise is in fact fulfilled.
  MarkPromiseSettled();
}

MockCdmSessionPromise::MockCdmSessionPromise(bool expect_success,
                                             std::string* new_session_id) {
  if (expect_success) {
    EXPECT_CALL(*this, resolve(_)).WillOnce(SaveArg<0>(new_session_id));
    EXPECT_CALL(*this, reject(_, _, _)).Times(0);
  } else {
    EXPECT_CALL(*this, resolve(_)).Times(0);
    EXPECT_CALL(*this, reject(_, _, NotEmpty()));
  }
}

MockCdmSessionPromise::~MockCdmSessionPromise() {
  // The EXPECT calls will verify that the promise is in fact fulfilled.
  MarkPromiseSettled();
}

MockCdmKeyStatusPromise::MockCdmKeyStatusPromise(
    bool expect_success,
    CdmKeyInformation::KeyStatus* key_status,
    CdmPromise::Exception* exception) {
  if (expect_success) {
    EXPECT_CALL(*this, resolve(_)).WillOnce(SaveArg<0>(key_status));
    EXPECT_CALL(*this, reject(_, _, _)).Times(0);
  } else {
    EXPECT_CALL(*this, resolve(_)).Times(0);
    if (exception) {
      EXPECT_CALL(*this, reject(_, _, NotEmpty()))
          .WillOnce(SaveArg<0>(exception));
    } else {
      EXPECT_CALL(*this, reject(_, _, NotEmpty()));
    }
  }
}

MockCdmKeyStatusPromise::~MockCdmKeyStatusPromise() {
  // The EXPECT calls will verify that the promise is in fact fulfilled.
  MarkPromiseSettled();
}

MockCdm::MockCdm() = default;

MockCdm::MockCdm(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  Initialize(cdm_config, session_message_cb, session_closed_cb,
             session_keys_change_cb, session_expiration_update_cb);
}

MockCdm::~MockCdm() = default;

void MockCdm::Initialize(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  key_system_ = cdm_config.key_system;
  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
}

void MockCdm::CallSessionMessageCB(const std::string& session_id,
                                   CdmMessageType message_type,
                                   const std::vector<uint8_t>& message) {
  session_message_cb_.Run(session_id, message_type, message);
}

void MockCdm::CallSessionClosedCB(const std::string& session_id,
                                  CdmSessionClosedReason reason) {
  session_closed_cb_.Run(session_id, reason);
}

void MockCdm::CallSessionKeysChangeCB(const std::string& session_id,
                                      bool has_additional_usable_key,
                                      CdmKeysInfo keys_info) {
  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void MockCdm::CallSessionExpirationUpdateCB(const std::string& session_id,
                                            base::Time new_expiry_time) {
  session_expiration_update_cb_.Run(session_id, new_expiry_time);
}

MockCdmFactory::MockCdmFactory(scoped_refptr<MockCdm> mock_cdm)
    : mock_cdm_(mock_cdm) {}

MockCdmFactory::~MockCdmFactory() = default;

void MockCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  // If no key system specified, notify that Create() failed.
  if (cdm_config.key_system.empty()) {
    std::move(cdm_created_cb)
        .Run(nullptr, CreateCdmStatus::kUnsupportedKeySystem);
    return;
  }

  // Since there is a CDM, call |before_creation_cb_| first.
  if (before_creation_cb_)
    before_creation_cb_.Run();

  mock_cdm_->Initialize(cdm_config, session_message_cb, session_closed_cb,
                        session_keys_change_cb, session_expiration_update_cb);
  std::move(cdm_created_cb).Run(mock_cdm_, CreateCdmStatus::kSuccess);
}

void MockCdmFactory::SetBeforeCreationCB(
    base::RepeatingClosure before_creation_cb) {
  before_creation_cb_ = std::move(before_creation_cb);
}

MockStreamParser::MockStreamParser() = default;

MockStreamParser::~MockStreamParser() = default;

MockMediaClient::MockMediaClient() = default;

MockMediaClient::~MockMediaClient() = default;

MockVideoEncoderMetricsProvider::MockVideoEncoderMetricsProvider() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MockVideoEncoderMetricsProvider::~MockVideoEncoderMetricsProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MockDestroy();
}
}  // namespace media
