// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api_observer.h"

namespace extensions {

bool TestApiObserver::OnTestMessage(TestSendMessageFunction* function,
                                    const std::string& message) {
  return false;
}

}  // namespace extensions
