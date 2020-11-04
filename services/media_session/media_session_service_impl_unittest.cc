// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_session_service_impl.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaSessionImplTest : public testing::Test {
 public:
  MediaSessionImplTest() = default;
  ~MediaSessionImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImplTest);
};

TEST_F(MediaSessionImplTest, InstantiateService) {
  MediaSessionServiceImpl service;
}

}  // namespace media_session
