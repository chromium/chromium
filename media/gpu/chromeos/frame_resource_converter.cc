// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/frame_resource_converter.h"

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/macros.h"

namespace media {

FrameResourceConverter::FrameResourceConverter() = default;

FrameResourceConverter::~FrameResourceConverter() = default;

void FrameResourceConverter::Initialize(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    OutputCB output_cb) {
  parent_task_runner_ = std::move(parent_task_runner);
  output_cb_ = std::move(output_cb);
}

void FrameResourceConverter::ConvertFrame(scoped_refptr<FrameResource> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  ConvertFrameImpl(std::move(frame));
}

void FrameResourceConverter::AbortPendingFrames() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  AbortPendingFramesImpl();
}

void FrameResourceConverter::AbortPendingFramesImpl() {}

bool FrameResourceConverter::HasPendingFrames() const {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  return HasPendingFramesImpl();
}

bool FrameResourceConverter::HasPendingFramesImpl() const {
  return false;
}

bool FrameResourceConverter::UsesGetOriginalFrameCB() const {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  return UsesGetOriginalFrameCBImpl();
}

bool FrameResourceConverter::UsesGetOriginalFrameCBImpl() const {
  return false;
}

void FrameResourceConverter::set_get_original_frame_cb(
    GetOriginalFrameCB get_original_frame_cb) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  get_original_frame_cb_ = std::move(get_original_frame_cb);
}

void FrameResourceConverter::OnError(const base::Location& location,
                                     const std::string& msg) {
  VLOGF(1) << "(" << location.ToString() << ") " << msg;

  // Currently we don't have a dedicated callback to notify client that error
  // occurs. Output a null frame to indicate any error occurs.
  // TODO(akahuang): Create an error notification callback.
  parent_task_runner_->PostTask(FROM_HERE, base::BindOnce(output_cb_, nullptr));
}

FrameResource* FrameResourceConverter::GetOriginalFrame(
    FrameResource& frame) const {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  if (!get_original_frame_cb_.is_null()) {
    return get_original_frame_cb_.Run(frame.GetSharedMemoryId());
  }
  return &frame;
}

const scoped_refptr<base::SequencedTaskRunner>&
FrameResourceConverter::parent_task_runner() {
  return parent_task_runner_;
}

void FrameResourceConverter::Output(scoped_refptr<VideoFrame> frame) const {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  output_cb_.Run(std::move(frame));
}

void FrameResourceConverter::Destroy() {
  DCHECK(!parent_task_runner_ ||
         parent_task_runner_->RunsTasksInCurrentSequence());
  delete this;
}

}  // namespace media

namespace std {

void default_delete<media::FrameResourceConverter>::operator()(
    media::FrameResourceConverter* ptr) const {
  ptr->Destroy();
}

}  // namespace std
