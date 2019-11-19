// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGE_STRUCTS_H_
#define CONTENT_COMMON_FRAME_MESSAGE_STRUCTS_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT FrameMsg_ViewChanged_Params {
  FrameMsg_ViewChanged_Params();
  ~FrameMsg_ViewChanged_Params();

  viz::FrameSinkId frame_sink_id;
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_MESSAGE_STRUCTS_H_
