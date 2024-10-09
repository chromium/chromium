// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BytesConsumerTestUtil::MockBytesConsumer::MockBytesConsumer() {
  using testing::_;
  using testing::ByMove;
  using testing::DoAll;
  using testing::Return;
  using testing::SetArgReferee;

  ON_CALL(*this, BeginRead(_))
      .WillByDefault(DoAll(SetArgReferee<0>(base::span<const char>{}),
                           Return(Result::kError)));
  ON_CALL(*this, EndRead(_)).WillByDefault(Return(Result::kError));
  ON_CALL(*this, GetPublicState()).WillByDefault(Return(PublicState::kErrored));
  ON_CALL(*this, DrainAsBlobDataHandle(_))
      .WillByDefault(Return(ByMove(nullptr)));
  ON_CALL(*this, DrainAsDataPipe())
      .WillByDefault(Return(ByMove(mojo::ScopedDataPipeConsumerHandle())));
  ON_CALL(*this, DrainAsFormData()).WillByDefault(Return(ByMove(nullptr)));
}

String BytesConsumerTestUtil::CharVectorToString(const Vector<char>& v) {
  return String(v.data(), v.size());
}

}  // namespace blink
