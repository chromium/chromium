// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_MOJO_WEB_TEST_HELPER_H_
#define CONTENT_WEB_TEST_BROWSER_MOJO_WEB_TEST_HELPER_H_

#include <string>

#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class MojoWebTestHelper : public mojom::MojoWebTestHelper {
 public:
  MojoWebTestHelper();

  MojoWebTestHelper(const MojoWebTestHelper&) = delete;
  MojoWebTestHelper& operator=(const MojoWebTestHelper&) = delete;

  ~MojoWebTestHelper() override;

  static void Create(mojo::PendingReceiver<mojom::MojoWebTestHelper> receiver);

  // mojom::MojoWebTestHelper:
  void Reverse(const std::string& message, ReverseCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_MOJO_WEB_TEST_HELPER_H_
