// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_STREAM_WRAPPER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_STREAM_WRAPPER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <wrl.h>
#include <queue>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"

namespace media {

namespace {

struct MediaFoundationSubsampleEntry {
  MediaFoundationSubsampleEntry(SubsampleEntry entry)
      : clear_bytes(entry.clear_bytes), cipher_bytes(entry.cypher_bytes) {}
  MediaFoundationSubsampleEntry() = default;
  DWORD clear_bytes = 0;
  DWORD cipher_bytes = 0;
};

}  // namespace

// IMFMediaStream implementation
// (https://msdn.microsoft.com/en-us/windows/desktop/ms697561) based on the
// given |demuxer_stream|.
//
class MediaFoundationStreamWrapper
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaStream> {
 public:
  MediaFoundationStreamWrapper();
  ~MediaFoundationStreamWrapper() override;

  static HRESULT Create(int stream_id,
                        IMFMediaSource* parent_source,
                        DemuxerStream* demuxer_stream,
                        scoped_refptr<base::SequencedTaskRunner> task_runner,
                        MediaFoundationStreamWrapper** stream_out);

  HRESULT RuntimeClassInitialize(int stream_id,
                                 IMFMediaSource* parent_source,
                                 DemuxerStream* demuxer_stream);
  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);
  void DetachParent();
  void DetachDemuxerStream();
  bool HasEnded() const;

  // The following methods can be invoked by media stack thread or MF threadpool
  // thread via MediaFoundationSourceWrapper::Start().
  void SetSelected(bool selected);
  bool IsSelected();
  bool IsEnabled();
  void SetEnabled(bool enabled);
  void SetFlushed(bool flushed);

  // TODO: revisting inheritance and potentially replacing it with composition.

  // The stream is encrypted or not.
  virtual bool IsEncrypted() const = 0;
  // Let derived class to adjust the IMFSample if necessary.
  virtual HRESULT TransformSample(Microsoft::WRL::ComPtr<IMFSample>& sample);
  // Allow derived class to tell us if we can send MEStreamFormatChanged to MF.
  virtual bool AreFormatChangesEnabled();

  HRESULT QueueStartedEvent(const PROPVARIANT* start_position);
  HRESULT QueueSeekedEvent(const PROPVARIANT* start_position);
  HRESULT QueueStoppedEvent();
  HRESULT QueuePausedEvent();
  HRESULT QueueFormatChangedEvent();
  DemuxerStream::Type StreamType() const;
  void ProcessRequestsIfPossible();
  void OnDemuxerStreamRead(DemuxerStream::Status status,
                           scoped_refptr<DecoderBuffer> buffer);

  // IMFMediaStream implementation - it is in general running in MF threadpool
  // thread.
  IFACEMETHODIMP GetMediaSource(IMFMediaSource** media_source_out) override;
  IFACEMETHODIMP GetStreamDescriptor(
      IMFStreamDescriptor** stream_descriptor_out) override;
  IFACEMETHODIMP RequestSample(IUnknown* token) override;

  // IMFMediaEventGenerator implementation - IMFMediaStream derives from
  // IMFMediaEventGenerator.
  IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** event_out) override;
  IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* callback,
                               IUnknown* state) override;
  IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result,
                             IMFMediaEvent** event_out) override;
  IFACEMETHODIMP QueueEvent(MediaEventType type,
                            REFGUID extended_type,
                            HRESULT status,
                            const PROPVARIANT* value) override;

  GUID GetLastKeyId() const;

 protected:
  HRESULT GenerateStreamDescriptor();
  HRESULT GenerateSampleFromDecoderBuffer(DecoderBuffer* buffer,
                                          IMFSample** sample_out);
  HRESULT ServiceSampleRequest(IUnknown* token, DecoderBuffer* buffer)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Returns true when a sample request has been serviced.
  bool ServicePostFlushSampleRequest();
  virtual HRESULT GetMediaType(IMFMediaType** media_type_out) = 0;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  enum class State {
    kInitialized,
    kStarted,
    kStopped,
    kPaused
  } state_ = State::kInitialized;
  DemuxerStream* demuxer_stream_ = nullptr;
  DemuxerStream::Type stream_type_ = DemuxerStream::Type::UNKNOWN;

  // Need exclusive access to some members between calls from MF threadpool
  // thread and calling thread from Chromium media stack.
  base::Lock lock_;

  // Indicates whether the stream is selected in the MF pipeline.
  bool selected_ GUARDED_BY(lock_) = false;

  // Indicates whether the stream is enabled in the Chromium media pipeline.
  bool enabled_ GUARDED_BY(lock_) = true;

  // Indicates whether the Chromium pipeline has flushed the renderer
  // (prior to a seek).
  // Since SetFlushed() can be invoked by media stack thread or MF threadpool
  // thread, |flushed_| and |post_flush_buffers_| are protected by lock.
  bool flushed_ GUARDED_BY(lock_) = false;

  int stream_id_;

  // |mf_media_event_queue_| is safe to be called on any thread.
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mf_media_event_queue_;
  Microsoft::WRL::ComPtr<IMFStreamDescriptor> mf_stream_descriptor_;

  // The IMFMediaSource that contains this stream.
  Microsoft::WRL::ComPtr<IMFMediaSource> parent_source_ GUARDED_BY(lock_);

  // If non-zero, there are pending sample request from MF.
  std::queue<Microsoft::WRL::ComPtr<IUnknown>> pending_sample_request_tokens_
      GUARDED_BY(lock_);

  // If true, there is a pending a read completion from Chromium media stack.
  bool pending_stream_read_ = false;

  bool stream_ended_ = false;
  GUID last_key_id_ = GUID_NULL;

  // Save media::DecoderBuffer from OnDemuxerStreamRead call when we are in
  // progress of a flush operation.
  std::queue<scoped_refptr<DecoderBuffer>> post_flush_buffers_
      GUARDED_BY(lock_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationStreamWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_STREAM_WRAPPER_H_
