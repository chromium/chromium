// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_stream_wrapper.h"

#include <mferror.h>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/win/mf_helpers.h"
#include "media/renderers/win/media_foundation_audio_stream.h"
#include "media/renderers/win/media_foundation_source_wrapper.h"
#include "media/renderers/win/media_foundation_video_stream.h"

namespace media {

using Microsoft::WRL::ComPtr;

namespace {

// |guid_string| is a binary serialization of a GUID in network byte order
// format.
GUID GetGUIDFromString(const std::string& guid_string) {
  DCHECK_EQ(guid_string.length(), sizeof(GUID));

  GUID guid = *(reinterpret_cast<UNALIGNED const GUID*>(guid_string.c_str()));
  guid.Data1 = _byteswap_ulong(guid.Data1);
  guid.Data2 = _byteswap_ushort(guid.Data2);
  guid.Data3 = _byteswap_ushort(guid.Data3);
  return guid;
}

bool AreLowIVBytesZero(const std::string& iv) {
  if (iv.length() != 16)
    return false;

  for (size_t i = 8; i < iv.length(); i++) {
    if (iv[i] != '\0') {
      return false;
    }
  }
  return true;
}

// Add encryption related attributes to |mf_sample| and update |last_key_id|.
HRESULT AddEncryptAttributes(const DecryptConfig& decrypt_config,
                             IMFSample* mf_sample,
                             GUID* last_key_id) {
  DVLOG(3) << __func__;

  MFSampleEncryptionProtectionScheme mf_protection_scheme;
  if (decrypt_config.encryption_scheme() == EncryptionScheme::kCenc) {
    mf_protection_scheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR;
  } else if (decrypt_config.encryption_scheme() == EncryptionScheme::kCbcs) {
    mf_protection_scheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CBC;
  } else {
    NOTREACHED() << "Unexpected encryption scheme";
    return MF_E_UNEXPECTED;
  }
  RETURN_IF_FAILED(mf_sample->SetUINT32(
      MFSampleExtension_Encryption_ProtectionScheme, mf_protection_scheme));

  // KID
  // https://matroska.org/technical/specs/index.html#ContentEncKeyID
  // For WebM case, key ID size is not specified.
  if (decrypt_config.key_id().length() != sizeof(GUID)) {
    DLOG(ERROR) << __func__ << ": Unsupported key ID size";
    return MF_E_UNEXPECTED;
  }
  GUID key_id = GetGUIDFromString(decrypt_config.key_id());
  RETURN_IF_FAILED(mf_sample->SetGUID(MFSampleExtension_Content_KeyID, key_id));
  *last_key_id = key_id;

  // IV
  size_t iv_length = decrypt_config.iv().length();
  DCHECK(iv_length == 16);
  // For cases where a 16-byte IV is specified, but the low 8-bytes are all
  // 0, ensure that a 8-byte IV is set (this allows HWDRM to work on
  // hardware / drivers which don't support CTR decryption with 16-byte IVs)
  if (AreLowIVBytesZero(decrypt_config.iv()))
    iv_length = 8;
  RETURN_IF_FAILED(mf_sample->SetBlob(
      MFSampleExtension_Encryption_SampleID,
      reinterpret_cast<const uint8_t*>(decrypt_config.iv().c_str()),
      iv_length));

  // Handle subsample entries.
  const auto& subsample_entries = decrypt_config.subsamples();
  if (subsample_entries.empty())
    return S_OK;

  std::vector<MediaFoundationSubsampleEntry> mf_subsample_entries(
      subsample_entries.size());
  for (size_t i = 0; i < subsample_entries.size(); i++) {
    mf_subsample_entries[i] =
        MediaFoundationSubsampleEntry(subsample_entries[i]);
  }
  const uint32_t mf_sample_entries_size =
      sizeof(MediaFoundationSubsampleEntry) * mf_subsample_entries.size();
  RETURN_IF_FAILED(mf_sample->SetBlob(
      MFSampleExtension_Encryption_SubSample_Mapping,
      reinterpret_cast<const uint8_t*>(mf_subsample_entries.data()),
      mf_sample_entries_size));

  return S_OK;
}

// MFTIME defines units of 100 nanoseconds.
MFTIME TimeDeltaToMfTime(base::TimeDelta time) {
  return time.InNanoseconds() / 100;
}

PendingInputBuffer::PendingInputBuffer(DemuxerStream::Status status,
                                       scoped_refptr<DecoderBuffer> buffer)
    : status(status), buffer(std::move(buffer)) {}

PendingInputBuffer::PendingInputBuffer(DemuxerStream::Status status)
    : status(status) {}

PendingInputBuffer::PendingInputBuffer(const PendingInputBuffer& other) =
    default;

PendingInputBuffer::~PendingInputBuffer() = default;

}  // namespace

MediaFoundationStreamWrapper::MediaFoundationStreamWrapper() = default;
MediaFoundationStreamWrapper::~MediaFoundationStreamWrapper() = default;

/*static*/
HRESULT MediaFoundationStreamWrapper::Create(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaFoundationStreamWrapper** stream_out) {
  DVLOG(1) << __func__ << ": stream_id=" << stream_id;

  ComPtr<MediaFoundationStreamWrapper> stream;
  switch (demuxer_stream->type()) {
    case DemuxerStream::Type::VIDEO:
      RETURN_IF_FAILED(MediaFoundationVideoStream::Create(
          stream_id, parent_source, demuxer_stream, std::move(media_log),
          &stream));
      break;
    case DemuxerStream::Type::AUDIO:
      RETURN_IF_FAILED(MediaFoundationAudioStream::Create(
          stream_id, parent_source, demuxer_stream, std::move(media_log),
          &stream));
      break;
    default:
      DLOG(ERROR) << "Unsupported demuxer stream type: "
                  << demuxer_stream->type();
      return E_INVALIDARG;
  }
  stream->SetTaskRunner(std::move(task_runner));
  *stream_out = stream.Detach();
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::RuntimeClassInitialize(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    std::unique_ptr<MediaLog> media_log) {
  {
    base::AutoLock auto_lock(lock_);
    parent_source_ = parent_source;
  }
  demuxer_stream_ = demuxer_stream;
  stream_id_ = stream_id;
  stream_type_ = demuxer_stream_->type();

  DVLOG_FUNC(1) << "stream_id=" << stream_id
                << ", stream_type=" << DemuxerStream::GetTypeName(stream_type_);

  media_log_ = std::move(media_log);
  if (base::FeatureList::IsEnabled(kMediaFoundationBatchRead)) {
    if (kBatchReadCount.Get() < 1 || kBatchReadCount.Get() > 500) {
      DLOG(WARNING) << "batch_read_count_=" << kBatchReadCount.Get()
                    << " is out of range [1,500], "
                    << (kBatchReadCount.Get() < 1
                            ? "it shouldn't be negative or 0"
                            : "it will spend more time "
                              "on writing and reading and maybe impacts UX")
                    << "; setting batch_read_count_=1";
      batch_read_count_ = 1;
    } else {
      batch_read_count_ = kBatchReadCount.Get();
    }
  }
  DVLOG_FUNC(1) << "batch_read_count_=" << batch_read_count_;

  RETURN_IF_FAILED(GenerateStreamDescriptor());
  RETURN_IF_FAILED(MFCreateEventQueue(&mf_media_event_queue_));
  return S_OK;
}

void MediaFoundationStreamWrapper::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DVLOG_FUNC(1);

  task_runner_ = std::move(task_runner);
}

void MediaFoundationStreamWrapper::DetachParent() {
  DVLOG_FUNC(1);

  base::AutoLock auto_lock(lock_);
  parent_source_ = nullptr;
}

void MediaFoundationStreamWrapper::DetachDemuxerStream() {
  DVLOG_FUNC(1);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  demuxer_stream_ = nullptr;
}

void MediaFoundationStreamWrapper::SetSelected(bool selected) {
  DVLOG_FUNC(2) << "selected=" << selected;

  base::AutoLock auto_lock(lock_);
  selected_ = selected;
}

bool MediaFoundationStreamWrapper::IsSelected() {
  base::AutoLock auto_lock(lock_);
  DVLOG_FUNC(2) << "selected_=" << selected_;

  return selected_;
}

bool MediaFoundationStreamWrapper::IsEnabled() {
  base::AutoLock auto_lock(lock_);
  DVLOG_FUNC(2) << "enabled_=" << enabled_;

  return enabled_;
}

void MediaFoundationStreamWrapper::SetEnabled(bool enabled) {
  DVLOG_FUNC(2) << "enabled=" << enabled;

  {
    base::AutoLock auto_lock(lock_);
    if (enabled_ == enabled)
      return;
    enabled_ = enabled;
  }
  // Restart processing of queued requests when stream is re-enabled.
  ProcessRequestsIfPossible();
}

void MediaFoundationStreamWrapper::SetFlushed(bool flushed) {
  DVLOG_FUNC(2) << "flushed=" << flushed;

  base::AutoLock auto_lock(lock_);
  flushed_ = flushed;
  if (flushed_) {
    DVLOG_FUNC(2) << "flush buffer_queue_";
    buffer_queue_.clear();
    while (!post_flush_buffers_.empty()) {
      post_flush_buffers_.pop();
    }
  }
}

bool MediaFoundationStreamWrapper::HasEnded() const {
  DVLOG_FUNC(2) << "stream_ended_=" << stream_ended_;

  return stream_ended_;
}

void MediaFoundationStreamWrapper::SetLastStartPosition(
    const PROPVARIANT* start_position) {
  // Events such as MF_MEDIA_ENGINE_EVENT_SEEKED may send a start position
  // with VT_EMPTY, this event should be ignored since it is not a valid start
  // time. Only VT_I8 will be used based on start of presentation:
  // https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-imfmediasession-start
  if (start_position->vt == VT_I8) {
    base::AutoLock auto_lock(lock_);
    last_start_time_ = start_position->hVal.QuadPart;
  }
}

HRESULT MediaFoundationStreamWrapper::QueueStartedEvent(
    const PROPVARIANT* start_position) {
  DVLOG_FUNC(2);

  // Save the new start position in the stream.
  SetLastStartPosition(start_position);

  state_ = State::kStarted;
  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamVar(
      MEStreamStarted, GUID_NULL, S_OK, start_position));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::QueueSeekedEvent(
    const PROPVARIANT* start_position) {
  DVLOG_FUNC(2);

  // Save the new start position in the stream.
  SetLastStartPosition(start_position);

  state_ = State::kStarted;
  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamVar(
      MEStreamSeeked, GUID_NULL, S_OK, start_position));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::QueueStoppedEvent() {
  DVLOG_FUNC(2);

  state_ = State::kStopped;
  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamVar(
      MEStreamStopped, GUID_NULL, S_OK, nullptr));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::QueuePausedEvent() {
  DVLOG_FUNC(2);

  state_ = State::kPaused;
  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamVar(
      MEStreamPaused, GUID_NULL, S_OK, nullptr));
  return S_OK;
}

DemuxerStream::Type MediaFoundationStreamWrapper::StreamType() const {
  return stream_type_;
}

void MediaFoundationStreamWrapper::ProcessRequestsIfPossible() {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  {
    base::AutoLock auto_lock(lock_);

    if (state_ == State::kPaused || !enabled_)
      return;

    if (pending_sample_request_tokens_.empty()) {
      return;
    }
  }

  if (ServicePostFlushSampleRequest()) {
    // A sample has been consumed from the |post_flush_buffers_|.
    return;
  }

  if (!demuxer_stream_) {
    return;
  }

  base::AutoLock auto_lock(lock_);
  if (!buffer_queue_.empty()) {
    // Using queued buffer for multi buffers read from Renderer process. If
    // a valid buffer already exists in queued buffer, return the buffer
    // directly without IPC calls for buffer requested from MediaEngine.
    OnDemuxerStreamRead(buffer_queue_.front().status,
                        std::move(buffer_queue_.front().buffer));
    buffer_queue_.pop_front();
    return;
  }

  // Request multi buffers by sending IPC to 'MojoDemuxerStreamImpl'.
  if (!pending_stream_read_) {
    DVLOG_FUNC(3) << " IPC send, batch_read_count_=" << batch_read_count_;
    TRACE_EVENT2("media", "MFGetBuffersFromRendererByIPC",
                 "StreamType:", DemuxerStream::GetTypeName(stream_type_),
                 "batch_read_count_:", batch_read_count_);
    pending_stream_read_ = true;
    demuxer_stream_->Read(
        batch_read_count_,
        base::BindOnce(
            &MediaFoundationStreamWrapper::OnDemuxerStreamReadBuffers,
            weak_factory_.GetWeakPtr()));
  }
}

void MediaFoundationStreamWrapper::OnDemuxerStreamReadBuffers(
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DVLOG_FUNC(3) << "receive data, status="
                << DemuxerStream::GetStatusName(status)
                << ", buffer count= " << buffers.size()
                << ", stream type=" << DemuxerStream::GetTypeName(stream_type_);
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(pending_stream_read_);
    pending_stream_read_ = false;

    DemuxerStream::DecoderBufferVector pending_buffers =
        (status == DemuxerStream::Status::kOk)
            ? std::move(buffers)
            : DemuxerStream::DecoderBufferVector{nullptr};
    for (auto& buffer : pending_buffers) {
      DVLOG_FUNC(3) << "push buffer to buffer_queue_, status="
                    << DemuxerStream::GetStatusName(status) << ", buffer="
                    << (buffer ? buffer->AsHumanReadableString(false) : "null");
      buffer_queue_.emplace_back(PendingInputBuffer(status, std::move(buffer)));
    }
  }

  // Restart processing of queued requests when we receive buffers.
  ProcessRequestsIfPossible();
}

HRESULT MediaFoundationStreamWrapper::ServiceSampleRequest(
    IUnknown* token,
    DecoderBuffer* buffer) {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  if (buffer->end_of_stream()) {
    if (!enabled_) {
      DVLOG_FUNC(2) << "Ignoring EOS for disabled stream";
      // token not dropped to reflect an outstanding request that stream wrapper
      // should service when the stream is enabled
      return S_OK;
    }
    DVLOG_FUNC(2) << "End of stream";
    RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamUnk(
        MEEndOfStream, GUID_NULL, S_OK, nullptr));
    stream_ended_ = true;
    if (parent_source_) {
      static_cast<MediaFoundationSourceWrapper*>(parent_source_.Get())
          ->CheckForEndOfPresentation();
    }
  } else {
    DVLOG_FUNC(3) << "buffer ts=" << buffer->timestamp()
                  << ", is_key_frame=" << buffer->is_key_frame();
    ComPtr<IMFSample> mf_sample;
    RETURN_IF_FAILED(GenerateSampleFromDecoderBuffer(buffer, &mf_sample));
    if (token) {
      RETURN_IF_FAILED(mf_sample->SetUnknown(MFSampleExtension_Token, token));
    }

    RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamUnk(
        MEMediaSample, GUID_NULL, S_OK, mf_sample.Get()));
  }

  pending_sample_request_tokens_.pop();

  return S_OK;
}

bool MediaFoundationStreamWrapper::ServicePostFlushSampleRequest() {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  HRESULT hr = S_OK;

  base::AutoLock auto_lock(lock_);
  if (flushed_ && state_ == State::kStarted &&
      last_start_time_ != kInvalidTime) {
    // Video may freeze during consecutive backward seek since MF does not
    // cancel previous pending seek, while Chromium's source starts new seek
    // immediately. MF's seek finishes when a sample's timestamp is equal to
    // or greater than seek time. Thus it would cause video to freeze until
    // source send samples with timestamps matching the previous pending seek.

    // MEStreamTick event notifies a gap in data and notify downstream
    // components not to expect any data at the specified time, allowing
    // downstream components to cancel the first seek
    // https://learn.microsoft.com/en-us/windows/win32/medfound/mestreamtick

    // Stream ticks are continuously sent until flush completes to make sure
    // all downsteam components have been rid of stale samples.
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = last_start_time_;
    hr = mf_media_event_queue_->QueueEventParamVar(MEStreamTick, GUID_NULL,
                                                   S_OK, &var);
    if (FAILED(hr)) {
      // Failure would indicate mf no longer accepts events, such as when
      // shutdown is called, thus stream ticks should no longer be needed.
      DLOG(WARNING) << "Failed to queue stream tick: " << PrintHr(hr);
    }
    return false;

  } else if ((flushed_ && state_ != State::kStarted) ||
             post_flush_buffers_.empty()) {
    return false;
  }

  DCHECK(!pending_sample_request_tokens_.empty());
  ComPtr<IUnknown> request_token = pending_sample_request_tokens_.front();
  hr = ServiceSampleRequest(request_token.Get(),
                            post_flush_buffers_.front().get());
  if (FAILED(hr)) {
    DLOG(WARNING) << "Failed to service post flush sample: " << PrintHr(hr);
    return false;
  }

  post_flush_buffers_.pop();
  return true;
}

HRESULT MediaFoundationStreamWrapper::QueueFormatChangedEvent() {
  DVLOG_FUNC(2);

  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(GetMediaType(&media_type));
  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamUnk(
      MEStreamFormatChanged, GUID_NULL, S_OK, media_type.Get()));
  return S_OK;
}

void MediaFoundationStreamWrapper::OnDemuxerStreamRead(
    DemuxerStream::Status status,
    scoped_refptr<DecoderBuffer> buffer) {
  DVLOG_FUNC(3) << "status=" << status
                << (buffer ? " buffer=" + buffer->AsHumanReadableString(true)
                           : "");
  {
    lock_.AssertAcquired();
    ComPtr<IUnknown> token = pending_sample_request_tokens_.front();
    HRESULT hr = S_OK;

    if (status == DemuxerStream::Status::kOk) {
      if (!encryption_type_reported_) {
        encryption_type_reported_ = true;
        ReportEncryptionType(buffer);
      }

      // Push |buffer| to process later if needed. Otherwise, process it
      // immediately.
      if (flushed_ || !post_flush_buffers_.empty()) {
        DVLOG_FUNC(3) << "push buffer.";
        post_flush_buffers_.push(buffer);
      } else {
        hr = ServiceSampleRequest(token.Get(), buffer.get());
        if (FAILED(hr)) {
          DLOG(ERROR) << __func__
                      << ": ServiceSampleRequest failed: " << PrintHr(hr);
          return;
        }
      }
    } else if (status == DemuxerStream::Status::kConfigChanged) {
      DVLOG_FUNC(2) << "Stream config changed, AreFormatChangesEnabled="
                    << AreFormatChangesEnabled();
      if (AreFormatChangesEnabled()) {
        hr = QueueFormatChangedEvent();
        if (FAILED(hr)) {
          DLOG(ERROR) << __func__
                      << ": QueueFormatChangedEvent failed: " << PrintHr(hr);
          return;
        }
      } else {
        // GetMediaType() calls {audio,video}_decoder_config(), which is
        // required by DemuxerStream when kConfigChanged happens.
        ComPtr<IMFMediaType> media_type;
        hr = GetMediaType(&media_type);
        if (FAILED(hr)) {
          DLOG(ERROR) << __func__ << ": GetMediaType failed: " << PrintHr(hr);
          return;
        }
      }
    } else if (status == DemuxerStream::Status::kError) {
      DVLOG_FUNC(2) << "Stream read error";
      mf_media_event_queue_->QueueEventParamVar(
          MEError, GUID_NULL, MF_E_INVALID_STREAM_DATA, nullptr);
      return;
    } else if (status == DemuxerStream::Status::kAborted) {
      DVLOG_FUNC(2) << "Stream read aborted";
      // Continue to ProcessRequestsIfPossible() to satisfy pending sample
      // request by issuing DemuxerStream::Read() if necessary.
    } else {
      NOTREACHED() << "Unexpected demuxer stream status. status=" << status
                   << ", this=" << this;
    }
  }

  // ProcessRequestsIfPossible calls OnDemuxerStreamRead, OnDemuxerStreamRead
  // calls ProcessRequestsIfPossible, so use PostTask to avoid deadlock here.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationStreamWrapper::ProcessRequestsIfPossible,
                     weak_factory_.GetWeakPtr()));
}

HRESULT MediaFoundationStreamWrapper::GenerateSampleFromDecoderBuffer(
    DecoderBuffer* buffer,
    IMFSample** sample_out) {
  DVLOG_FUNC(3);

  ComPtr<IMFSample> mf_sample;
  RETURN_IF_FAILED(MFCreateSample(&mf_sample));

  if (buffer->is_key_frame()) {
    RETURN_IF_FAILED(mf_sample->SetUINT32(MFSampleExtension_CleanPoint, 1));
  }

  MFTIME sample_time = TimeDeltaToMfTime(buffer->timestamp());
  RETURN_IF_FAILED(mf_sample->SetSampleTime(sample_time));

  MFTIME sample_duration = TimeDeltaToMfTime(buffer->duration());
  RETURN_IF_FAILED(mf_sample->SetSampleDuration(sample_duration));

  ComPtr<IMFMediaBuffer> mf_buffer;
  size_t data_size = buffer->data_size();
  RETURN_IF_FAILED(MFCreateMemoryBuffer(buffer->data_size(), &mf_buffer));

  BYTE* mf_buffer_data = nullptr;
  DWORD max_length = 0;
  RETURN_IF_FAILED(mf_buffer->Lock(&mf_buffer_data, &max_length, 0));
  memcpy(mf_buffer_data, buffer->data(), data_size);
  RETURN_IF_FAILED(mf_buffer->SetCurrentLength(data_size));
  RETURN_IF_FAILED(mf_buffer->Unlock());

  RETURN_IF_FAILED(mf_sample->AddBuffer(mf_buffer.Get()));

  if (buffer->decrypt_config()) {
    RETURN_IF_FAILED(AddEncryptAttributes(*(buffer->decrypt_config()),
                                          mf_sample.Get(), &last_key_id_));
  }

  RETURN_IF_FAILED(TransformSample(mf_sample));

  *sample_out = mf_sample.Detach();
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::TransformSample(
    Microsoft::WRL::ComPtr<IMFSample>& sample) {
  DVLOG_FUNC(3);

  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::GetMediaSource(
    IMFMediaSource** media_source_out) {
  DVLOG_FUNC(2);
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  if (!parent_source_) {
    DLOG(ERROR) << __func__ << ": MF_E_SHUTDOWN";
    return MF_E_SHUTDOWN;
  }
  RETURN_IF_FAILED(parent_source_.CopyTo(media_source_out));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::GetStreamDescriptor(
    IMFStreamDescriptor** stream_descriptor_out) {
  DVLOG_FUNC(2);

  if (!mf_stream_descriptor_) {
    DLOG(ERROR) << __func__ << ": MF_E_NOT_INITIALIZED";
    return MF_E_NOT_INITIALIZED;
  }
  RETURN_IF_FAILED(mf_stream_descriptor_.CopyTo(stream_descriptor_out));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::RequestSample(IUnknown* token) {
  DVLOG_FUNC(3);
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  // If token is nullptr, we still want to push it to represent a sample
  // request from MF.
  pending_sample_request_tokens_.push(token);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationStreamWrapper::ProcessRequestsIfPossible,
                     weak_factory_.GetWeakPtr()));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::GetEvent(DWORD flags,
                                               IMFMediaEvent** event_out) {
  DVLOG_FUNC(3);
  DCHECK(mf_media_event_queue_);

  // Not tracing hr to avoid the noise from MF_E_NO_EVENTS_AVAILABLE.
  return mf_media_event_queue_->GetEvent(flags, event_out);
}

HRESULT MediaFoundationStreamWrapper::BeginGetEvent(IMFAsyncCallback* callback,
                                                    IUnknown* state) {
  DVLOG_FUNC(3);
  DCHECK(mf_media_event_queue_);

  RETURN_IF_FAILED(mf_media_event_queue_->BeginGetEvent(callback, state));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::EndGetEvent(IMFAsyncResult* result,
                                                  IMFMediaEvent** event_out) {
  DVLOG_FUNC(3);
  DCHECK(mf_media_event_queue_);

  RETURN_IF_FAILED(mf_media_event_queue_->EndGetEvent(result, event_out));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::QueueEvent(MediaEventType type,
                                                 REFGUID extended_type,
                                                 HRESULT status,
                                                 const PROPVARIANT* value) {
  DVLOG_FUNC(3);
  DCHECK(mf_media_event_queue_);

  RETURN_IF_FAILED(mf_media_event_queue_->QueueEventParamVar(
      type, extended_type, status, value));
  return S_OK;
}

HRESULT MediaFoundationStreamWrapper::GenerateStreamDescriptor() {
  DVLOG_FUNC(2);

  ComPtr<IMFMediaType> media_type;
  IMFMediaType** mediaTypes = &media_type;

  RETURN_IF_FAILED(GetMediaType(&media_type));
  RETURN_IF_FAILED(MFCreateStreamDescriptor(stream_id_, 1, mediaTypes,
                                            &mf_stream_descriptor_));

  if (IsEncrypted()) {
    RETURN_IF_FAILED(mf_stream_descriptor_->SetUINT32(MF_SD_PROTECTED, 1));
  }

  return S_OK;
}

bool MediaFoundationStreamWrapper::AreFormatChangesEnabled() {
  return true;
}

GUID MediaFoundationStreamWrapper::GetLastKeyId() const {
  return last_key_id_;
}

void MediaFoundationStreamWrapper::ReportEncryptionType(
    const scoped_refptr<DecoderBuffer>& buffer) {
  auto encryption_type = EncryptionType::kClear;
  if (IsEncrypted()) {
    bool is_buffer_encrypted = buffer->decrypt_config();
    encryption_type = !is_buffer_encrypted
                          ? EncryptionType::kEncryptedWithClearLead
                          : EncryptionType::kEncrypted;
  }

  if (encryption_type == EncryptionType::kEncryptedWithClearLead) {
    MEDIA_LOG(INFO, media_log_) << "MediaFoundationStreamWrapper: "
                                << DemuxerStream::GetTypeName(stream_type_)
                                << " stream is encrypted with clear lead";
  }

  // TODO(xhwang): Report `encryption_type` to `PipelineStatistics` so it's
  // also reported to UKM.
}

}  // namespace media
