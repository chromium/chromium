// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "build/chromeos_buildflags.h"
#include "media/base/cdm_context.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check does not account for BUILDFLAG(), so including these headers will
// make gn check fail for builds other than ash-chrome. See gn help nogncheck
// for more information.
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"  // nogncheck
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"  // nogncheck

namespace {
// This increments the lower 64 bit counter of an 128 bit IV.
void ctr128_inc64(uint8_t* counter) {
  uint32_t n = 16;
  do {
    if (++counter[--n] != 0)
      return;
  } while (n > 8);
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

VaapiVideoDecoderDelegate::VaapiVideoDecoderDelegate(
    VaapiDecodeSurfaceHandler* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    ProtectedSessionUpdateCB on_protected_session_update_cb,
    CdmContext* cdm_context,
    EncryptionScheme encryption_scheme)
    : vaapi_dec_(vaapi_dec),
      vaapi_wrapper_(std::move(vaapi_wrapper)),
      on_protected_session_update_cb_(
          std::move(on_protected_session_update_cb)),
      encryption_scheme_(encryption_scheme),
      protected_session_state_(ProtectedSessionState::kNotCreated),
      performing_recovery_(false) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (cdm_context)
    chromeos_cdm_context_ = cdm_context->GetChromeOsCdmContext();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  transcryption_ = cdm_context && VaapiWrapper::GetImplementationType() ==
                                      VAImplementation::kMesaGallium;
}

VaapiVideoDecoderDelegate::~VaapiVideoDecoderDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Also destroy the protected session on destruction of the accelerator
  // delegate. That way if a new delegate is created, when it tries to create a
  // new protected session it won't overwrite the existing one.
  vaapi_wrapper_->DestroyProtectedSession();
}

void VaapiVideoDecoderDelegate::set_vaapi_wrapper(
    scoped_refptr<VaapiWrapper> vaapi_wrapper) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  vaapi_wrapper_ = std::move(vaapi_wrapper);
  protected_session_state_ = ProtectedSessionState::kNotCreated;
  hw_identifier_.clear();
  hw_key_data_map_.clear();
}

void VaapiVideoDecoderDelegate::OnVAContextDestructionSoon() {}

bool VaapiVideoDecoderDelegate::HasInitiatedProtectedRecovery() {
  if (protected_session_state_ != ProtectedSessionState::kNeedsRecovery)
    return false;

  performing_recovery_ = true;
  protected_session_state_ = ProtectedSessionState::kNotCreated;
  return true;
}

bool VaapiVideoDecoderDelegate::SetDecryptConfig(
    std::unique_ptr<DecryptConfig> decrypt_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is possible to switch between clear and encrypted (and vice versa).
  if (!decrypt_config)
    return true;
  decrypt_config_ = std::move(decrypt_config);
  encryption_scheme_ = decrypt_config_->encryption_scheme();
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
VaapiVideoDecoderDelegate::ProtectedSessionState
VaapiVideoDecoderDelegate::SetupDecryptDecode(
    bool full_sample,
    size_t size,
    VAEncryptionParameters* crypto_params,
    std::vector<VAEncryptionSegmentInfo>* segments,
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(crypto_params);
  DCHECK(segments);
  if (protected_session_state_ == ProtectedSessionState::kInProcess ||
      protected_session_state_ == ProtectedSessionState::kFailed) {
    return protected_session_state_;
  }
  if (protected_session_state_ == ProtectedSessionState::kNotCreated) {
    if (!chromeos_cdm_context_) {
      LOG(ERROR) << "Cannot create protected session w/out ChromeOsCdmContext";
      protected_session_state_ = ProtectedSessionState::kFailed;
      return protected_session_state_;
    }
    // We need to start the creation of this, first part requires getting the
    // hw config data from the daemon.
    chromeos_cdm_context_->GetHwConfigData(base::BindPostTaskToCurrentDefault(
        base::BindOnce(&VaapiVideoDecoderDelegate::OnGetHwConfigData,
                       weak_factory_.GetWeakPtr())));
    protected_session_state_ = ProtectedSessionState::kInProcess;
    return protected_session_state_;
  }

  DCHECK_EQ(protected_session_state_, ProtectedSessionState::kCreated);

  const bool ctr = (encryption_scheme_ == EncryptionScheme::kCenc);
  if (ctr) {
    crypto_params->encryption_type = full_sample
                                         ? VA_ENCRYPTION_TYPE_FULLSAMPLE_CTR
                                         : VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR;
  } else {
    crypto_params->encryption_type = full_sample
                                         ? VA_ENCRYPTION_TYPE_FULLSAMPLE_CBC
                                         : VA_ENCRYPTION_TYPE_SUBSAMPLE_CBC;
  }

  // For multi-slice we may already have segment information in here, so
  // calculate the current offset.
  size_t offset = 0;
  for (const auto& segment : *segments)
    offset += segment.segment_length;

  if (subsamples.empty() ||
      (subsamples.size() == 1 && subsamples[0].cypher_bytes == 0)) {
    // We still need to specify the crypto params to the driver for some reason
    // and indicate the entire content is clear.
    VAEncryptionSegmentInfo segment_info = {};
    segment_info.segment_start_offset = offset;
    segment_info.segment_length = segment_info.init_byte_length = size;
    if (decrypt_config_) {
      // We need to specify the IV even if the segment is clear.
      memcpy(segment_info.aes_cbc_iv_or_ctr, decrypt_config_->iv().data(),
             DecryptConfig::kDecryptionKeySize);
    }
    segments->emplace_back(std::move(segment_info));
    crypto_params->num_segments++;
    crypto_params->segment_info = &segments->front();
    return protected_session_state_;
  }

  // On Intel if we change encryption modes after we have started decrypting
  // then we need to rebuild the protected session.
  if (!IsTranscrypted() &&
      last_used_encryption_scheme_ != EncryptionScheme::kUnencrypted &&
      last_used_encryption_scheme_ != encryption_scheme_) {
    LOG(WARNING) << "Forcing rebuild since encryption mode changed midstream";
    RecoverProtectedSession();
    last_used_encryption_scheme_ = EncryptionScheme::kUnencrypted;
    return protected_session_state_;
  }

  last_used_encryption_scheme_ = encryption_scheme_;
  DCHECK(decrypt_config_);
  // We also need to make sure we have the key data for the active
  // DecryptConfig now that the protected session exists.
  if (!base::Contains(hw_key_data_map_, decrypt_config_->key_id())) {
    DVLOG(1) << "Looking up the key data for: " << decrypt_config_->key_id();
    chromeos_cdm_context_->GetHwKeyData(
        decrypt_config_.get(), hw_identifier_,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &VaapiVideoDecoderDelegate::OnGetHwKeyData,
            weak_factory_.GetWeakPtr(), decrypt_config_->key_id())));
    // Don't change our state here because we are created, but we just return
    // kInProcess for now to trigger a wait/retry state.
    return ProtectedSessionState::kInProcess;
  }

  crypto_params->num_segments += subsamples.size();
  // If the pattern has no skip blocks, which means the entire thing is
  // encrypted, then don't specify a pattern at all as Intel's implementation
  // does not expect that.
  if (decrypt_config_->HasPattern() &&
      decrypt_config_->encryption_pattern()->skip_byte_block()) {
    crypto_params->blocks_stripe_encrypted =
        decrypt_config_->encryption_pattern()->crypt_byte_block();
    crypto_params->blocks_stripe_clear =
        decrypt_config_->encryption_pattern()->skip_byte_block();
  }
  size_t total_cypher_size = 0;
  std::vector<uint8_t> iv(DecryptConfig::kDecryptionKeySize);
  iv.assign(decrypt_config_->iv().begin(), decrypt_config_->iv().end());
  for (const auto& entry : subsamples) {
    VAEncryptionSegmentInfo segment_info = {};
    segment_info.segment_start_offset = offset;
    segment_info.segment_length = entry.clear_bytes + entry.cypher_bytes;
    memcpy(segment_info.aes_cbc_iv_or_ctr, iv.data(),
           DecryptConfig::kDecryptionKeySize);
    if (ctr) {
      size_t partial_block_size =
          (DecryptConfig::kDecryptionKeySize -
           (total_cypher_size % DecryptConfig::kDecryptionKeySize)) %
          DecryptConfig::kDecryptionKeySize;
      segment_info.partial_aes_block_size = partial_block_size;
      if (entry.cypher_bytes > partial_block_size) {
        // If we are finishing a block, increment the counter.
        if (partial_block_size)
          ctr128_inc64(iv.data());
        // Increment the counter for every complete block we are adding.
        for (size_t block = 0;
             block < (entry.cypher_bytes - partial_block_size) /
                         DecryptConfig::kDecryptionKeySize;
             ++block)
          ctr128_inc64(iv.data());
      }
      total_cypher_size += entry.cypher_bytes;
    }
    segment_info.init_byte_length = entry.clear_bytes;
    offset += entry.clear_bytes + entry.cypher_bytes;
    segments->emplace_back(std::move(segment_info));
  }
  memcpy(crypto_params->wrapped_decrypt_blob,
         hw_key_data_map_[decrypt_config_->key_id()].data(),
         DecryptConfig::kDecryptionKeySize);
  crypto_params->key_blob_size = DecryptConfig::kDecryptionKeySize;
  crypto_params->segment_info = &segments->front();
  return protected_session_state_;
}
#endif  // if BUILDFLAG(IS_CHROMEOS_ASH)

bool VaapiVideoDecoderDelegate::NeedsProtectedSessionRecovery() {
  if (!IsEncryptedSession() || !vaapi_wrapper_->IsProtectedSessionDead() ||
      performing_recovery_) {
    return false;
  }

  RecoverProtectedSession();
  return true;
}

void VaapiVideoDecoderDelegate::ProtectedDecodedSucceeded() {
  performing_recovery_ = false;
}

std::string VaapiVideoDecoderDelegate::GetDecryptKeyId() const {
  DCHECK(decrypt_config_);
  return decrypt_config_->key_id();
}

void VaapiVideoDecoderDelegate::OnGetHwConfigData(
    bool success,
    const std::vector<uint8_t>& config_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    protected_session_state_ = ProtectedSessionState::kFailed;
    on_protected_session_update_cb_.Run(false);
    return;
  }

  hw_identifier_.clear();
  if (!vaapi_wrapper_->CreateProtectedSession(encryption_scheme_, config_data,
                                              &hw_identifier_)) {
    LOG(ERROR) << "Failed to setup protected session";
    protected_session_state_ = ProtectedSessionState::kFailed;
    on_protected_session_update_cb_.Run(false);
    return;
  }

  protected_session_state_ = ProtectedSessionState::kCreated;
  on_protected_session_update_cb_.Run(true);
}

void VaapiVideoDecoderDelegate::OnGetHwKeyData(
    const std::string& key_id,
    Decryptor::Status status,
    const std::vector<uint8_t>& key_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There's a special case here where we are updating usage times/checking on
  // key validity, and in that case the key is already in the map.
  if (base::Contains(hw_key_data_map_, key_id)) {
    if (status == Decryptor::Status::kSuccess)
      return;
    // This key is no longer valid, decryption will fail, so stop playback
    // now. This key should have been renewed by the CDM instead.
    LOG(ERROR) << "CDM has lost key information, stopping playback";
    protected_session_state_ = ProtectedSessionState::kFailed;
    on_protected_session_update_cb_.Run(false);
    return;
  }
  if (status != Decryptor::Status::kSuccess) {
    // If it's a failure, then indicate so, otherwise if it's waiting for a key,
    // then we don't do anything since we will get called again when there's a
    // message about key availability changing.
    if (status == Decryptor::Status::kNoKey) {
      DVLOG(1) << "HW did not have key information, keep waiting for it";
      return;
    }
    LOG(ERROR) << "Failure getting the key data, fail overall";
    protected_session_state_ = ProtectedSessionState::kFailed;
    on_protected_session_update_cb_.Run(false);
    return;
  }
  if (key_data.size() != DecryptConfig::kDecryptionKeySize) {
    LOG(ERROR) << "Invalid key size returned of: " << key_data.size();
    protected_session_state_ = ProtectedSessionState::kFailed;
    on_protected_session_update_cb_.Run(false);
    return;
  }
  hw_key_data_map_[key_id] = key_data;
  on_protected_session_update_cb_.Run(true);
}

void VaapiVideoDecoderDelegate::RecoverProtectedSession() {
  LOG(WARNING) << "Protected session loss detected, initiating recovery";
  protected_session_state_ = ProtectedSessionState::kNeedsRecovery;
  hw_key_data_map_.clear();
  hw_identifier_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(chromeos_cdm_context_);
  // ARC will not re-seek, so we cannot do the VAContext recreation for it.
  if (!chromeos_cdm_context_->UsingArcCdm()) {
    OnVAContextDestructionSoon();
    vaapi_wrapper_->DestroyContext();
  }
  vaapi_wrapper_->DestroyProtectedSession();
  if (chromeos_cdm_context_->UsingArcCdm()) {
    // The ARC decoder doesn't handle the WaitingCB that'll get invoked so we
    // need to trigger a protected update ourselves in order to get decoding
    // running again.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindRepeating(on_protected_session_update_cb_, true));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace media
