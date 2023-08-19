// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_output_queue.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/codec_picture.h"

namespace media {

VideoToolboxOutputQueue::VideoToolboxOutputQueue(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DVLOG(1) << __func__;
}

VideoToolboxOutputQueue::~VideoToolboxOutputQueue() {
  DVLOG(1) << __func__;
}

void VideoToolboxOutputQueue::SetOutputCB(
    const VideoDecoder::OutputCB output_cb) {
  DVLOG(2) << __func__;
  output_cb_ = std::move(output_cb);
}

void VideoToolboxOutputQueue::SchedulePicture(
    scoped_refptr<CodecPicture> picture) {
  DVLOG(3) << __func__;
  scheduled_pictures_.push(std::move(picture));
  Process();
}

void VideoToolboxOutputQueue::FulfillPicture(
    scoped_refptr<CodecPicture> picture,
    scoped_refptr<VideoFrame> frame) {
  DVLOG(3) << __func__;
  DCHECK(!fulfilled_pictures_.contains(picture));
  DCHECK(output_cb_);
  fulfilled_pictures_[picture] = std::move(frame);
  Process();
}

void VideoToolboxOutputQueue::Flush(VideoDecoder::DecodeCB flush_cb) {
  DVLOG(2) << __func__;
  DCHECK(!flush_cb_);
  flush_cb_ = std::move(flush_cb);
  Process();
}

void VideoToolboxOutputQueue::Reset(DecoderStatus status) {
  DVLOG(2) << __func__;

  if (flush_cb_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(flush_cb_), status));
  }

  scheduled_pictures_ = {};
  fulfilled_pictures_.clear();
}

void VideoToolboxOutputQueue::Process() {
  DVLOG(4) << __func__;

  // Output scheduled pictures that have been fulfilled.
  while (!scheduled_pictures_.empty()) {
    scoped_refptr<CodecPicture>& picture = scheduled_pictures_.front();
    auto fulfilled_it = fulfilled_pictures_.find(picture);
    if (fulfilled_it == fulfilled_pictures_.end()) {
      break;
    }

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(output_cb_, std::move(fulfilled_it->second)));

    fulfilled_pictures_.erase(fulfilled_it);
    scheduled_pictures_.pop();
  }

  // Check if an outstanding flush is complete.
  if (flush_cb_ && scheduled_pictures_.empty()) {
    DCHECK(fulfilled_pictures_.empty());
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(flush_cb_), DecoderStatus::Codes::kOk));
  }
}

}  // namespace media
