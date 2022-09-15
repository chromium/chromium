// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/persistence/mock_keychain.h"

using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace remoting {

MockKeychain::MockKeychain() {}

MockKeychain::~MockKeychain() {}

void MockKeychain::ExpectAndCaptureSetData(Key key,
                                           const std::string& account,
                                           std::string* out_data) {
  EXPECT_CALL(*this, SetData(key, account, _)).WillOnce(SaveArg<2>(out_data));
}

void MockKeychain::ExpectGetDataAndReturn(Key key,
                                          const std::string& account,
                                          const std::string& data_to_return) {
  EXPECT_CALL(*this, GetData(key, account)).WillOnce(Return(data_to_return));
}

}  // namespace remoting
