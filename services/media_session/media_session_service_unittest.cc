// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_session_service.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaSessionTest : public testing::Test {
 public:
  MediaSessionTest() = default;
  ~MediaSessionTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionTest);
};

TEST_F(MediaSessionTest, InstantiateService) {
  MediaSessionService service(nullptr);
}

}  // namespace media_session
