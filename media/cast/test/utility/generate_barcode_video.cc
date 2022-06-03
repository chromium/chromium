// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <cstdlib>

#include "base/check_op.h"
#include "media/base/video_frame.h"
#include "media/cast/test/utility/barcode.h"

void DumpPlane(scoped_refptr<media::VideoFrame> frame,
               int plane) {
  for (int row = 0; row < frame->rows(plane); row++) {
    CHECK_EQ(static_cast<size_t>(frame->row_bytes(plane)),
             fwrite(frame->data(plane) + frame->stride(plane) * row,
                    1,
                    frame->row_bytes(plane),
                    stdout));
  }
}

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "Usage: generate_barcode_video "
            "<width> <height> <fps> <frames> >output.y4m\n");
    exit(1);
  }
  int width = atoi(argv[1]);
  int height = atoi(argv[2]);
  int fps = atoi(argv[3]);
  uint16_t wanted_frames = atoi(argv[4]);
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(width, height));
  printf("YUV4MPEG2 W%d H%d F%d:1 Ip C420mpeg2\n", width, height, fps);
  for (uint16_t frame_id = 1; frame_id <= wanted_frames; frame_id++) {
    CHECK(media::cast::test::EncodeBarcode(frame_id, frame));
    printf("FRAME\n");
    DumpPlane(frame, media::VideoFrame::kYPlane);
    DumpPlane(frame, media::VideoFrame::kUPlane);
    DumpPlane(frame, media::VideoFrame::kVPlane);
  }
}
