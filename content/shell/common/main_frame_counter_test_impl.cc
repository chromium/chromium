// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/main_frame_counter_test_impl.h"
#include "base/no_destructor.h"
#include "content/common/main_frame_counter.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

MainFrameCounterTestImpl* GetMainFrameCounterTestImpl() {
  static base::NoDestructor<MainFrameCounterTestImpl> instance;
  return instance.get();
}

MainFrameCounterTestImpl::MainFrameCounterTestImpl() = default;
MainFrameCounterTestImpl::~MainFrameCounterTestImpl() = default;

// static
void MainFrameCounterTestImpl::Bind(
    mojo::PendingReceiver<mojom::MainFrameCounterTest> receiver) {
  GetMainFrameCounterTestImpl()->receiver_.Bind(std::move(receiver));
}

void MainFrameCounterTestImpl::HasMainFrame(HasMainFrameCallback callback) {
  std::move(callback).Run(MainFrameCounter::has_main_frame());
}

}  // namespace content
