// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_MOCK_OPENSCREEN_ENVIRONMENT_H_
#define MEDIA_CAST_TEST_MOCK_OPENSCREEN_ENVIRONMENT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"

namespace media::cast {

// An openscreen::cast::Environment that can intercept all packet sends, for
// unit testing.
class MockOpenscreenEnvironment : public openscreen::cast::Environment {
 public:
  MockOpenscreenEnvironment(openscreen::ClockNowFunctionPtr now_function,
                            openscreen::TaskRunner& task_runner);
  ~MockOpenscreenEnvironment() override;

  // Used to return fake values, to simulate a bound socket for testing.
  MOCK_METHOD(openscreen::IPEndpoint,
              GetBoundLocalEndpoint,
              (),
              (const, override));

  // Used for intercepting packet sends from the implementation under test.
  MOCK_METHOD(void,
              SendPacket,
              (openscreen::ByteView packet,
               openscreen::cast::PacketMetadata metadata),
              (override));
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_MOCK_OPENSCREEN_ENVIRONMENT_H_
