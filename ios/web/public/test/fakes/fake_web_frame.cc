// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_web_frame.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "ios/web/public/thread/web_task_traits.h"

namespace web {

// FakeMainWebFrame
FakeMainWebFrame::FakeMainWebFrame(GURL security_origin)
    : FakeWebFrame(kMainFakeFrameId, /*is_main_frame=*/true, security_origin) {}

FakeMainWebFrame::~FakeMainWebFrame() {}

// FakeChildWebFrame
FakeChildWebFrame::FakeChildWebFrame(GURL security_origin)
    : FakeWebFrame(kChildFakeFrameId,
                   /*is_main_frame=*/false,
                   security_origin) {}

FakeChildWebFrame::~FakeChildWebFrame() {}

}  // namespace web
