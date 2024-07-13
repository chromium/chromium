// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/fuchsia/fuchsia_stream_decryptor.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/drm/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"

namespace media {
namespace {

std::string GetEncryptionScheme(EncryptionScheme mode) {
  switch (mode) {
    case EncryptionScheme::kCenc:
      return fuchsia::media::ENCRYPTION_SCHEME_CENC;
    case EncryptionScheme::kCbcs:
      return fuchsia::media::ENCRYPTION_SCHEME_CBCS;
    default:
      NOTREACHED_IN_MIGRATION()
          << "unknown encryption mode " << static_cast<int>(mode);
      return "";
  }
}

std::vector<fuchsia::media::SubsampleEntry> GetSubsamples(
    const std::vector<SubsampleEntry>& subsamples) {
  std::vector<fuchsia::media::SubsampleEntry> fuchsia_subsamples(
      subsamples.size());

  for (size_t i = 0; i < subsamples.size(); i++) {
    fuchsia_subsamples[i].clear_bytes = subsamples[i].clear_bytes;
    fuchsia_subsamples[i].encrypted_bytes = subsamples[i].cypher_bytes;
  }

  return fuchsia_subsamples;
}

fuchsia::media::EncryptionPattern GetEncryptionPattern(
    EncryptionPattern pattern) {
  fuchsia::media::EncryptionPattern fuchsia_pattern;
  fuchsia_pattern.clear_blocks = pattern.skip_byte_block();
  fuchsia_pattern.encrypted_blocks = pattern.crypt_byte_block();
  return fuchsia_pattern;
}

fuchsia::media::FormatDetails GetClearFormatDetails() {
  fuchsia::media::EncryptedFormat encrypted_format;
  encrypted_format.set_scheme(fuchsia::media::ENCRYPTION_SCHEME_UNENCRYPTED)
      .set_subsamples({})
      .set_init_vector({});

  fuchsia::media::FormatDetails format;
  format.set_format_details_version_ordinal(0);
  format.mutable_domain()->crypto().set_encrypted(std::move(encrypted_format));
  return format;
}

fuchsia::media::FormatDetails GetEncryptedFormatDetails(
    const DecryptConfig* config) {
  DCHECK(config);

  fuchsia::media::EncryptedFormat encrypted_format;
  encrypted_format.set_scheme(GetEncryptionScheme(config->encryption_scheme()))
      .set_key_id(std::vector<uint8_t>(config->key_id().begin(),
                                       config->key_id().end()))
      .set_init_vector(
          std::vector<uint8_t>(config->iv().begin(), config->iv().end()))
      .set_subsamples(GetSubsamples(config->subsamples()));
  if (config->encryption_scheme() == EncryptionScheme::kCbcs) {
    DCHECK(config->encryption_pattern().has_value());
    encrypted_format.set_pattern(
        GetEncryptionPattern(config->encryption_pattern().value()));
  }

  fuchsia::media::FormatDetails format;
  format.set_format_details_version_ordinal(0);
  format.mutable_domain()->crypto().set_encrypted(std::move(encrypted_format));
  return format;
}

}  // namespace

FuchsiaStreamDecryptor::FuchsiaStreamDecryptor(
    fuchsia::media::StreamProcessorPtr processor)
    : processor_(std::move(processor), this),
      allocator_("CrFuchsiaStreamDecryptor") {}

FuchsiaStreamDecryptor::~FuchsiaStreamDecryptor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::RepeatingClosure FuchsiaStreamDecryptor::GetOnNewKeyClosure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &FuchsiaStreamDecryptor::OnNewKey, weak_factory_.GetWeakPtr()));
}

void FuchsiaStreamDecryptor::Initialize(Sink* sink,
                                        size_t min_buffer_size,
                                        size_t min_buffer_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sink_ = sink;

  min_buffer_size_ = min_buffer_size;
  min_buffer_count_ = min_buffer_count;

  input_buffer_collection_ = allocator_.AllocateNewCollection();
  input_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::SetInputBufferCollectionToken,
                     base::Unretained(&processor_)));
  auto buffer_constraints = VmoBuffer::GetRecommendedConstraints(
      kInputBufferCount, min_buffer_size_, /*writable=*/true);
  input_buffer_collection_->Initialize(std::move(buffer_constraints),
                                       "CrFuchsiaStreamDecryptor");
  input_buffer_collection_->AcquireBuffers(base::BindOnce(
      &FuchsiaStreamDecryptor::OnInputBuffersAcquired, base::Unretained(this)));
}

void FuchsiaStreamDecryptor::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  input_writer_queue_.EnqueueBuffer(std::move(buffer));
}

void FuchsiaStreamDecryptor::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Close current stream and drop all the cached decoder buffers.
  // Keep input and output buffers to avoid buffer re-allocation.
  processor_.Reset();
  input_writer_queue_.ResetQueue();
  waiting_for_key_ = false;
}

void FuchsiaStreamDecryptor::OnStreamProcessorAllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_buffer_collection_ = allocator_.AllocateNewCollection();
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::CompleteOutputBuffersAllocation,
                     base::Unretained(&processor_)));
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&Sink::OnSysmemBufferStreamBufferCollectionToken,
                     base::Unretained(sink_)));

  fuchsia::sysmem2::BufferCollectionConstraints constraints;
  constraints.mutable_usage()->set_none(fuchsia::sysmem2::NONE_USAGE);
  constraints.set_min_buffer_count(min_buffer_count_);
  auto& memory_constraints = *constraints.mutable_buffer_memory_constraints();
  memory_constraints.set_min_size_bytes(min_buffer_size_);
  memory_constraints.set_ram_domain_supported(true);
  memory_constraints.set_cpu_domain_supported(true);
  memory_constraints.set_inaccessible_domain_supported(true);

  output_buffer_collection_->Initialize(std::move(constraints),
                                        "CrFuchsiaStreamDecryptorOutput");
}

void FuchsiaStreamDecryptor::OnStreamProcessorEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sink_->OnSysmemBufferStreamEndOfStream();
}

void FuchsiaStreamDecryptor::OnStreamProcessorOutputFormat(
    fuchsia::media::StreamOutputFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FuchsiaStreamDecryptor::OnStreamProcessorOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sink_->OnSysmemBufferStreamOutputPacket(std::move(packet));
}

void FuchsiaStreamDecryptor::OnStreamProcessorNoKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!waiting_for_key_);

  // Reset stream position, but keep all pending buffers. They will be
  // resubmitted later, when we have a new key.
  input_writer_queue_.ResetPositionAndPause();

  if (retry_on_no_key_event_) {
    retry_on_no_key_event_ = false;
    input_writer_queue_.Unpause();
    return;
  }

  waiting_for_key_ = true;
  sink_->OnSysmemBufferStreamNoKey();
}

void FuchsiaStreamDecryptor::OnStreamProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnError();
}

void FuchsiaStreamDecryptor::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Reset();

  // No need to reset other fields since OnError() is called for non-recoverable
  // errors.

  sink_->OnSysmemBufferStreamError();
}

void FuchsiaStreamDecryptor::OnInputBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffers.empty()) {
    OnError();
    return;
  }

  input_writer_queue_.Start(
      std::move(buffers),
      base::BindRepeating(&FuchsiaStreamDecryptor::SendInputPacket,
                          base::Unretained(this)),
      base::BindRepeating(&FuchsiaStreamDecryptor::ProcessEndOfStream,
                          base::Unretained(this)));
}

void FuchsiaStreamDecryptor::SendInputPacket(
    const DecoderBuffer* buffer,
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!packet.unit_end()) {
    // The encrypted data size is too big. Decryptor should consider
    // splitting the buffer and update the IV and subsample entries.
    // TODO(crbug.com/42050011): Handle large encrypted buffer correctly. For
    // now, just reject the decryption.
    LOG(ERROR) << "DecoderBuffer doesn't fit in one packet.";
    OnError();
    return;
  }

  fuchsia::media::FormatDetails format =
      (buffer->decrypt_config())
          ? GetEncryptedFormatDetails(buffer->decrypt_config())
          : GetClearFormatDetails();

  packet.set_format(std::move(format));
  processor_.Process(std::move(packet));
}

void FuchsiaStreamDecryptor::ProcessEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processor_.ProcessEos();
}

void FuchsiaStreamDecryptor::OnNewKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!waiting_for_key_) {
    retry_on_no_key_event_ = true;
    return;
  }

  DCHECK(!retry_on_no_key_event_);
  waiting_for_key_ = false;
  input_writer_queue_.Unpause();
}

}  // namespace media
