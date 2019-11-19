// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/fuzzer_support.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

static content::Env* env;

bool Initialize() {
  blink::WebRuntimeFeatures::EnableDisplayLocking(true);
  env = new content::Env();
  return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool initialized = Initialize();
  // Suppress unused variable warning.
  (void)initialized;

  // Only handle reasonable size inputs.
  if (size < 1 || size > 10000)
    return 0;

  std::string data_as_string(reinterpret_cast<const char*>(data), size);
  int num_rafs = std::hash<std::string>()(data_as_string) % 10;
  env->adapter->LoadHTML(data_as_string, "");

  // Delay each frame 17ms which is roughly the length of a frame when running
  // at 60fps.
  auto frame_delay = base::TimeDelta::FromMillisecondsD(17);

  for (int i = 0; i < num_rafs; ++i) {
    base::RunLoop run_loop;
    blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), frame_delay);
    run_loop.Run();

    env->adapter->GetMainFrame()
        ->View()
        ->MainFrameWidget()
        ->UpdateAllLifecyclePhases(
            blink::WebWidget::LifecycleUpdateReason::kTest);
  }
  return 0;
}
