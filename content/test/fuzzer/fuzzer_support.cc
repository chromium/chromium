// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/fuzzer_support.h"

#include <string>

#include "base/feature_list.h"
#include "base/i18n/icu_util.h"
#include "base/test/test_timeouts.h"
#include "gin/v8_initializer.h"
#include "third_party/blink/public/platform/web_runtime_features.h"

namespace content {

namespace {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
#endif
}

void RenderViewTestAdapter::SetUp() {
  RenderViewTest::SetUp();
  CreateFakeWebURLLoaderFactory();
}

Env::Env() {
  base::CommandLine::Init(0, nullptr);
  base::FeatureList::InitializeInstance(std::string(), std::string());
  base::i18n::InitializeICU();
  TestTimeouts::Initialize();

  blink::WebRuntimeFeatures::EnableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  adapter = std::make_unique<RenderViewTestAdapter>();
  adapter->SetUp();
}

Env::~Env() {
  LOG(FATAL) << "NOT SUPPORTED";
}

}  // namespace content
