// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_
#define CONTENT_SHELL_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_

#include "content/shell/common/main_frame_counter_test.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class MainFrameCounterTestImpl final : public mojom::MainFrameCounterTest {
 public:
  MainFrameCounterTestImpl();
  ~MainFrameCounterTestImpl() override;
  static void Bind(mojo::PendingReceiver<mojom::MainFrameCounterTest> receiver);

  void HasMainFrame(HasMainFrameCallback) override;

 private:
  mojo::Receiver<mojom::MainFrameCounterTest> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_
