// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_track_source.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace remoting {
namespace protocol {

class WebrtcVideoTrackSourceTest : public testing::Test {
 public:
  WebrtcVideoTrackSourceTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(WebrtcVideoTrackSourceTest, AddSinkTriggersCallback) {
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source =
      new rtc::RefCountedObject<WebrtcVideoTrackSource>(
          base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE));
  source->AddOrUpdateSink(nullptr, rtc::VideoSinkWants());

  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace protocol
}  // namespace remoting
