// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/avfoundation_output_stream.h"

#import <AVFoundation/AVFoundation.h>
#include <CoreAudio/CoreAudio.h>

#include "base/apple/osstatus_logging.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/mac/channel_layout_util_mac.h"
#include "media/base/sample_format.h"

namespace media {

namespace {
base::apple::ScopedCFTypeRef<CMBlockBufferRef> CreateBlockBuffer(
    AudioBus& bus,
    int frames_filled) {
  size_t data_size = frames_filled * bus.channels() * sizeof(float);

  base::apple::ScopedCFTypeRef<CMBlockBufferRef> block_buffer;
  OSStatus status = CMBlockBufferCreateEmpty(
      /*structureAllocator=*/kCFAllocatorDefault,
      /*subBlockCapacity=*/0,
      /*flags=*/0,
      /*blockBufferOut=*/block_buffer.InitializeInto());

  if (status != kCMBlockBufferNoErr) {
    OSSTATUS_LOG(ERROR, status) << "CMBlockBufferCreateEmpty failed.";
    return base::apple::ScopedCFTypeRef<CMBlockBufferRef>(nullptr);
  }

  status = CMBlockBufferAppendMemoryBlock(
      /*theBuffer=*/block_buffer.get(),
      /*memoryBlock=*/nullptr,
      /*blockLength=*/data_size,
      /*blockAllocator=*/kCFAllocatorDefault,
      /*customBlockSource=*/nullptr,
      /*offsetToData=*/0,
      /*dataLength=*/data_size,
      /*flags=*/kCMBlockBufferAssureMemoryNowFlag);

  if (status != kCMBlockBufferNoErr) {
    OSSTATUS_LOG(ERROR, status) << "CMBlockBufferAppendMemoryBlock failed.";
    return base::apple::ScopedCFTypeRef<CMBlockBufferRef>(nullptr);
  }

  char* data_ptr;
  status = CMBlockBufferGetDataPointer(
      /*theBuffer=*/block_buffer.get(),
      /*offset=*/0,
      /*lengthAtOffsetOut=*/nullptr,
      /*totalLengthOut=*/nullptr,
      /*dataPointerOut=*/&data_ptr);

  if (status != kCMBlockBufferNoErr || !data_ptr) {
    OSSTATUS_LOG(ERROR, status) << "CMBlockBufferGetDataPointer failed.";
    return base::apple::ScopedCFTypeRef<CMBlockBufferRef>(nullptr);
  }

  bus.ToInterleaved<Float32SampleTypeTraits>(
      frames_filled, reinterpret_cast<float*>(data_ptr));
  return block_buffer;
}

}  // namespace

struct AVFoundationOutputStream::ObjCStorage {
  AVSampleBufferAudioRenderer* __strong renderer;
  AVSampleBufferRenderSynchronizer* __strong synchronizer;
  dispatch_queue_t __strong queue;
};

AVFoundationOutputStream::AVFoundationOutputStream(
    AudioManagerMac* audio_manager,
    const AudioParameters& params,
    std::string_view device_uid)
    : audio_manager_(*audio_manager),
      params_(params),
      device_uid_(device_uid),
      callback_(nullptr),
      objc_storage_(std::make_unique<ObjCStorage>()),
      audio_bus_(AudioBus::Create(params)) {
  DVLOG(1)
      << __func__
      << ": Initializing AVFoundationOutputStream with these AudioParameters:: "
      << params.AsHumanReadableString();
}

AVFoundationOutputStream::~AVFoundationOutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AVFoundationOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  objc_storage_->renderer = [[AVSampleBufferAudioRenderer alloc] init];
  if (!objc_storage_->renderer) {
    LOG(ERROR) << "Failed to create AVSampleBufferAudioRenderer.";
    return false;
  }

  objc_storage_->synchronizer = [[AVSampleBufferRenderSynchronizer alloc] init];
  if (!objc_storage_->synchronizer) {
    LOG(ERROR) << "Failed to create AVSampleBufferRenderSynchronizer.";
    return false;
  }

  objc_storage_->queue = dispatch_queue_create(
      "com.chromium.media.AVFoundationOutputStream",
      dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                              QOS_CLASS_USER_INTERACTIVE, 0));
  if (!objc_storage_->queue) {
    LOG(ERROR) << "Failed to create dispatch queue.";
    return false;
  }

  objc_storage_->renderer.audioOutputDeviceUniqueID =
      base::SysUTF8ToNSString(device_uid_);

  [objc_storage_->synchronizer addRenderer:objc_storage_->renderer];

  AudioStreamBasicDescription asbd = {
      .mSampleRate = static_cast<Float64>(params_.sample_rate()),
      .mFormatID = kAudioFormatLinearPCM,
      .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
      .mFramesPerPacket = 1,
      .mChannelsPerFrame = static_cast<UInt32>(params_.channels()),
      .mBitsPerChannel = 8 * sizeof(float),
  };
  asbd.mBytesPerFrame = asbd.mChannelsPerFrame * asbd.mBitsPerChannel / 8;
  asbd.mBytesPerPacket = asbd.mFramesPerPacket * asbd.mBytesPerFrame;

  auto scoped_layout = ChannelLayoutToAudioChannelLayout(
      params_.channel_layout(), params_.channels());

  OSStatus status = CMAudioFormatDescriptionCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*asbd=*/&asbd,
      /*layoutSize=*/scoped_layout->layout_size(),
      /*layout=*/scoped_layout->layout(),
      /*magicCookieSize=*/0,
      /*magicCookie=*/nullptr,
      /*extensions=*/nullptr,
      /*formatDescriptionOut=*/format_description_.InitializeInto());

  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "CMAudioFormatDescriptionCreate failed.";
    return false;
  }

  return true;
}

void AVFoundationOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  {
    base::AutoLock al(lock_);
    CHECK(!callback_);
    callback_ = callback;
  }

  objc_storage_->synchronizer.rate = 1.0;

  [objc_storage_->renderer requestMediaDataWhenReadyOnQueue:objc_storage_->queue
                                                 usingBlock:^{
                                                   FeedCallback();
                                                 }];
}

void AVFoundationOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dispatch_sync(objc_storage_->queue, ^{
    [objc_storage_->renderer stopRequestingMediaData];
    [objc_storage_->renderer flush];
    objc_storage_->synchronizer.rate = 0.0;
  });

  base::AutoLock al(lock_);
  if (!callback_) {
    return;
  }
  callback_ = nullptr;
}

void AVFoundationOutputStream::Flush() {}

void AVFoundationOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();

  // Inform the manager that we have been closed.
  audio_manager_->ReleaseOutputStream(this);
}

void AVFoundationOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(objc_storage_->renderer);
  objc_storage_->renderer.volume = std::clamp(volume, 0.0, 1.0);
}

void AVFoundationOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(objc_storage_->renderer);
  *volume = objc_storage_->renderer.volume;
}

void AVFoundationOutputStream::FeedCallback() {
  base::AutoLock al(lock_);

  if (!callback_) {
    return;
  }

  int frames_filled =
      callback_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(),
                            AudioGlitchInfo(), audio_bus_.get());

  CHECK_GE(frames_filled, 0);

  if (frames_filled == 0) {
    // If the callback provided zero frames, we provide a silent buffer to avoid
    // audio glitches.
    audio_bus_->Zero();
    frames_filled = audio_bus_->frames();
  }

  CMTime current_renderer_time = objc_storage_->synchronizer.currentTime;

  base::apple::ScopedCFTypeRef<CMBlockBufferRef> block_buffer =
      CreateBlockBuffer(*audio_bus_, frames_filled);
  if (!block_buffer) {
    HandleError();
    return;
  }
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  CMSampleTimingInfo timing_info = {
      .duration = CMTimeMake(frames_filled, params_.sample_rate()),
      .presentationTimeStamp = current_renderer_time,
      .decodeTimeStamp = kCMTimeInvalid};

  OSStatus status = CMSampleBufferCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*dataBuffer=*/block_buffer.get(),
      /*dataReady=*/true,
      /*makeDataReadyCallback=*/nil,
      /*makeDataReadyRefcon=*/nil,
      /*formatDescription=*/format_description_.get(),
      /*numSamples=*/frames_filled,
      /*numSampleTimingEntries=*/1,
      /*sampleTimingArray=*/&timing_info,
      /*numSampleSizeEntries=*/0,
      /*sampleSizeArray=*/nil,
      /*sampleBufferOut=*/sample_buffer.InitializeInto());

  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "CMSampleBufferCreate failed.";
    HandleError();
    return;
  }

  [objc_storage_->renderer enqueueSampleBuffer:sample_buffer.get()];

  if (objc_storage_->renderer.status ==
      AVQueuedSampleBufferRenderingStatusFailed) {
    LOG(ERROR) << "AVSampleBufferAudioRenderer failed: "
               << base::SysNSStringToUTF8(
                      [objc_storage_->renderer.error localizedDescription]);
    HandleError();
    return;
  }
}

void AVFoundationOutputStream::HandleError() {
  base::AutoLock al(lock_);
  if (callback_) {
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }
}

}  // namespace media
