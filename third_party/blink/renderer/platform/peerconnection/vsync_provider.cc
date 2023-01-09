// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/vsync_provider.h"
#include <memory>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"

namespace blink {

// This class provides a VideoFrameSinkBundle BeginFrameObserver
// implementation which gives access to VSyncs and VSyncs enabled signals.
// After construction, this class can only be operated on the video frame
// compositor thread.
class VSyncProviderImpl::BeginFrameObserver
    : public VideoFrameSinkBundle::BeginFrameObserver {
 public:
  using VSyncEnabledCallback = base::RepeatingCallback<void(bool /*enabled*/)>;

  explicit BeginFrameObserver(VSyncEnabledCallback vsync_enabled_callback)
      : vsync_enabled_callback_(std::move(vsync_enabled_callback)) {}

  // Requests to be called back once on the next vsync.
  void RequestVSyncCallback(base::OnceClosure callback) {
    vsync_callback_ = std::move(callback);
  }

  // Returns a weak ptr to be dereferenced only on the video frame compositor
  // thread.
  base::WeakPtr<BeginFrameObserver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // VideoFrameSinkBundle::BeginFrameObserver overrides.
  void OnBeginFrameCompletion() override {
    if (vsync_callback_)
      std::move(vsync_callback_).Run();
  }
  void OnBeginFrameCompletionEnabled(bool enabled) override {
    vsync_enabled_callback_.Run(enabled);
  }

 private:
  base::OnceClosure vsync_callback_;
  VSyncEnabledCallback vsync_enabled_callback_;
  base::WeakPtrFactory<BeginFrameObserver> weak_factory_{this};
};

VSyncProviderImpl::VSyncProviderImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    uint32_t frame_sink_client_id)
    : task_runner_(task_runner), frame_sink_client_id_(frame_sink_client_id) {}

void VSyncProviderImpl::SetVSyncCallback(base::OnceClosure callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure callback,
                        base::WeakPtr<BeginFrameObserver> observer) {
                       if (observer) {
                         observer->RequestVSyncCallback(std::move(callback));
                       }
                     },
                     std::move(callback), weak_observer_));
}

void VSyncProviderImpl::Initialize(
    base::RepeatingCallback<void(bool /*visible*/)> vsync_enabled_callback) {
  auto observer =
      std::make_unique<BeginFrameObserver>(std::move(vsync_enabled_callback));
  weak_observer_ = observer->GetWeakPtr();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](uint32_t client_id, std::unique_ptr<BeginFrameObserver> observer) {
            VideoFrameSinkBundle::GetOrCreateSharedInstance(client_id)
                .SetBeginFrameObserver(std::move(observer));
          },
          frame_sink_client_id_, std::move(observer)));
}

}  // namespace blink
