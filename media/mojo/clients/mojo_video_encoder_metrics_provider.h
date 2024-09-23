// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include <atomic>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// MojoVideoEncoderMetricsProviderFactory creates VideoEncoderMetricsProvider
// that records encoder statistics in browser process via mojo call.
// MojoVideoEncoderMetricsProviderFactory constructor can be called on any
// sequence. CreateVideoEncoderMetricsProvider() must be called on the same
// specific sequence.
// The created VideoEncoderMetricsProviders must be operated and destroyed on a
// specific sequence and MojoVideoEncoderMetricsProviderFactory needs to be
// destroyed on the sequence.
// If MojoVideoEncoderMetricsProviderFactory is created one-off, it is
// guaranteed that the factory is destroyed with VideoEncoderMetricsProvider.
// For example, |factory| below will be destroyed when |metrics_provider| is
// destroyed.
// ```
// auto factory = base::MakeRefCounted<
//     MojoVideoEncoderMetricsProviderFactory>(kCastMirroring, pending_remote);
// auto metrics_provider =
//     std::move(factory).CreateVideoEncoderMetricsProvider();
// ```
// If MojoVideoEncoderMetricsProviderFactory is held by some class, then it
// needs to ensure it is destroyed on the same sequence as one destroying
// created VideoEncoderMetricsProviders.
class MojoVideoEncoderMetricsProviderFactory
    : public base::RefCountedThreadSafe<
          MojoVideoEncoderMetricsProviderFactory> {
 public:
  MojoVideoEncoderMetricsProviderFactory(
      mojom::VideoEncoderUseCase use_case,
      mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote);

  // Virtual for unit tests like rtc_video_encoder_test.cc.
  virtual std::unique_ptr<VideoEncoderMetricsProvider>
  CreateVideoEncoderMetricsProvider();

 protected:
  friend class base::RefCountedThreadSafe<
      MojoVideoEncoderMetricsProviderFactory>;
  virtual ~MojoVideoEncoderMetricsProviderFactory();

  // For easily mocking in unit tests.
  explicit MojoVideoEncoderMetricsProviderFactory(
      mojom::VideoEncoderUseCase use_case);

 private:
  class MojoVideoEncoderMetricsProvider;

  // GetRemote() is called by MojoVideoEncoderMetricsProvider.
  mojo::Remote<mojom::VideoEncoderMetricsProvider>* GetRemote();

  const mojom::VideoEncoderUseCase use_case_
      GUARDED_BY_CONTEXT(create_provider_sequence_checker_);
  std::atomic_uint64_t encoder_id_
      GUARDED_BY_CONTEXT(create_provider_sequence_checker_){0};

  mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote_
      GUARDED_BY_CONTEXT(remote_sequence_checker_);
  mojo::Remote<mojom::VideoEncoderMetricsProvider> remote_
      GUARDED_BY_CONTEXT(remote_sequence_checker_);

  SEQUENCE_CHECKER(remote_sequence_checker_);
  SEQUENCE_CHECKER(create_provider_sequence_checker_);
};
}  // namespace media
#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
