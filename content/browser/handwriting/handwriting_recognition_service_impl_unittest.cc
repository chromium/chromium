// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_impl.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/handwriting/handwriting_recognizer_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

class HandwritingRecognitionServiceImplTest : public RenderViewHostTestHarness {
};

TEST_F(HandwritingRecognitionServiceImplTest, CreateHandwritingRecognizer) {
  mojo::Remote<handwriting::mojom::HandwritingRecognitionService> remote;
  HandwritingRecognitionServiceImpl::Create(
      remote.BindNewPipeAndPassReceiver());
  auto model_constraint = handwriting::mojom::HandwritingModelConstraint::New();
  bool is_callback_called = false;
  base::RunLoop runloop;
  remote->CreateHandwritingRecognizer(
      std::move(model_constraint),
      base::BindLambdaForTesting(
          [&](handwriting::mojom::CreateHandwritingRecognizerResult result,
              mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer>
                  remote) {
            EXPECT_EQ(
                result,
                handwriting::mojom::CreateHandwritingRecognizerResult::kError);
            EXPECT_TRUE(!remote);
            is_callback_called = true;
            runloop.Quit();
          }));
  runloop.Run();
  EXPECT_TRUE(is_callback_called);
}

TEST_F(HandwritingRecognitionServiceImplTest,
       QueryHandwritingRecognizerSupport) {
  mojo::Remote<handwriting::mojom::HandwritingRecognitionService> remote;
  HandwritingRecognitionServiceImpl::Create(
      remote.BindNewPipeAndPassReceiver());
  auto query = handwriting::mojom::HandwritingFeatureQuery::New();
  query->languages.push_back("en");
  query->alternatives = true;
  bool is_callback_called = false;
  base::RunLoop runloop;
  remote->QueryHandwritingRecognizerSupport(
      std::move(query),
      base::BindLambdaForTesting(
          [&](handwriting::mojom::HandwritingFeatureQueryResultPtr result) {
            EXPECT_FALSE(result.is_null());
            // We do not support anything here.
            EXPECT_EQ(
                result->languages,
                handwriting::mojom::HandwritingFeatureStatus::kNotSupported);
            EXPECT_EQ(
                result->alternatives,
                handwriting::mojom::HandwritingFeatureStatus::kNotSupported);
            EXPECT_EQ(
                result->segmentation_result,
                handwriting::mojom::HandwritingFeatureStatus::kNotQueried);
            is_callback_called = true;
            runloop.Quit();
          }));
  runloop.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace content
