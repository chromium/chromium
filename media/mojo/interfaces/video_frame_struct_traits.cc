// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/interfaces/video_frame_struct_traits.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

namespace {

media::mojom::VideoFrameDataPtr MakeVideoFrameData(
    const scoped_refptr<media::VideoFrame>& input) {
  if (input->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM)) {
    return media::mojom::VideoFrameData::NewEosData(
        media::mojom::EosVideoFrameData::New());
  }

  if (input->storage_type() == media::VideoFrame::STORAGE_MOJO_SHARED_BUFFER) {
    media::MojoSharedBufferVideoFrame* mojo_frame =
        static_cast<media::MojoSharedBufferVideoFrame*>(input.get());

    // TODO(https://crbug.com/803136): This should duplicate as READ_ONLY, but
    // can't because there is no guarantee that the input handle is sharable as
    // read-only.
    mojo::ScopedSharedBufferHandle dup = mojo_frame->Handle().Clone(
        mojo::SharedBufferHandle::AccessMode::READ_WRITE);
    DCHECK(dup.is_valid());

    return media::mojom::VideoFrameData::NewSharedBufferData(
        media::mojom::SharedBufferVideoFrameData::New(
            std::move(dup), mojo_frame->MappedSize(),
            mojo_frame->stride(media::VideoFrame::kYPlane),
            mojo_frame->stride(media::VideoFrame::kUPlane),
            mojo_frame->stride(media::VideoFrame::kVPlane),
            mojo_frame->PlaneOffset(media::VideoFrame::kYPlane),
            mojo_frame->PlaneOffset(media::VideoFrame::kUPlane),
            mojo_frame->PlaneOffset(media::VideoFrame::kVPlane)));
  }

  if (input->HasTextures()) {
    std::vector<gpu::MailboxHolder> mailbox_holder(
        media::VideoFrame::kMaxPlanes);
    size_t num_planes = media::VideoFrame::NumPlanes(input->format());
    for (size_t i = 0; i < num_planes; i++)
      mailbox_holder[i] = input->mailbox_holder(i);
    return media::mojom::VideoFrameData::NewMailboxData(
        media::mojom::MailboxVideoFrameData::New(std::move(mailbox_holder)));
  }

  NOTREACHED() << "Unsupported VideoFrame conversion";
  return nullptr;
}

}  // namespace

// static
media::mojom::VideoFrameDataPtr StructTraits<media::mojom::VideoFrameDataView,
                                             scoped_refptr<media::VideoFrame>>::
    data(const scoped_refptr<media::VideoFrame>& input) {
  return media::mojom::VideoFrameDataPtr(MakeVideoFrameData(input));
}

// static
bool StructTraits<media::mojom::VideoFrameDataView,
                  scoped_refptr<media::VideoFrame>>::
    Read(media::mojom::VideoFrameDataView input,
         scoped_refptr<media::VideoFrame>* output) {
  // View of the |data| member of the input media::mojom::VideoFrame.
  media::mojom::VideoFrameDataDataView data;
  input.GetDataDataView(&data);

  if (data.is_eos_data()) {
    *output = media::VideoFrame::CreateEOSFrame();
    return !!*output;
  }

  media::VideoPixelFormat format;
  if (!input.ReadFormat(&format))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  if (!gfx::Rect(coded_size).Contains(visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  base::TimeDelta timestamp;
  if (!input.ReadTimestamp(&timestamp))
    return false;

  scoped_refptr<media::VideoFrame> frame;
  if (data.is_shared_buffer_data()) {
    media::mojom::SharedBufferVideoFrameDataDataView shared_buffer_data;
    data.GetSharedBufferDataDataView(&shared_buffer_data);

    // TODO(sandersd): Conversion from uint64_t to size_t could cause
    // corruption. Platform-dependent types should be removed from the
    // implementation (limiting to 32-bit offsets is fine).
    frame = media::MojoSharedBufferVideoFrame::Create(
        format, coded_size, visible_rect, natural_size,
        shared_buffer_data.TakeFrameData(),
        shared_buffer_data.frame_data_size(), shared_buffer_data.y_offset(),
        shared_buffer_data.u_offset(), shared_buffer_data.v_offset(),
        shared_buffer_data.y_stride(), shared_buffer_data.u_stride(),
        shared_buffer_data.v_stride(), timestamp);
  } else if (data.is_mailbox_data()) {
    media::mojom::MailboxVideoFrameDataDataView mailbox_data;
    data.GetMailboxDataDataView(&mailbox_data);

    std::vector<gpu::MailboxHolder> mailbox_holder;
    if (!mailbox_data.ReadMailboxHolder(&mailbox_holder))
      return false;

    gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
    for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; i++)
      mailbox_holder_array[i] = mailbox_holder[i];

    frame = media::VideoFrame::WrapNativeTextures(
        format, mailbox_holder_array, media::VideoFrame::ReleaseMailboxCB(),
        coded_size, visible_rect, natural_size, timestamp);
  } else {
    // TODO(sandersd): Switch on the union tag to avoid this ugliness?
    NOTREACHED();
    return false;
  }

  if (!frame)
    return false;

  base::Value metadata;
  if (!input.ReadMetadata(&metadata))
    return false;

  frame->metadata()->MergeInternalValuesFrom(metadata);

  gfx::ColorSpace color_space;
  if (!input.ReadColorSpace(&color_space))
    return false;
  frame->set_color_space(color_space);

  *output = std::move(frame);
  return true;
}

}  // namespace mojo
