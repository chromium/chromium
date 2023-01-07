// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_session_service_impl.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaSessionImplTest : public testing::Test {
 public:
  MediaSessionImplTest() = default;

  MediaSessionImplTest(const MediaSessionImplTest&) = delete;
  MediaSessionImplTest& operator=(const MediaSessionImplTest&) = delete;

  ~MediaSessionImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MediaSessionImplTest, InstantiateService) {
  MediaSessionServiceImpl service;
}

}  // namespace media_session
