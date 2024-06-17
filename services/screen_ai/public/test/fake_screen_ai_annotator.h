// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_TEST_FAKE_SCREEN_AI_ANNOTATOR_H_
#define SERVICES_SCREEN_AI_PUBLIC_TEST_FAKE_SCREEN_AI_ANNOTATOR_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"

namespace screen_ai::test {

class FakeScreenAIAnnotator : public mojom::ScreenAIAnnotator {
 public:
  explicit FakeScreenAIAnnotator(bool create_empty_result);
  FakeScreenAIAnnotator(const FakeScreenAIAnnotator&) = delete;
  FakeScreenAIAnnotator& operator=(const FakeScreenAIAnnotator&) = delete;
  ~FakeScreenAIAnnotator() override;

  void PerformOcrAndReturnAXTreeUpdate(
      const ::SkBitmap& image,
      PerformOcrAndReturnAXTreeUpdateCallback callback) override;

  void PerformOcrAndReturnAnnotation(
      const ::SkBitmap& image,
      PerformOcrAndReturnAnnotationCallback callback) override;

  void SetClientType(mojom::OcrClientType client_type) override;

  mojo::PendingRemote<mojom::ScreenAIAnnotator> BindNewPipeAndPassRemote();

 private:
  mojo::Receiver<mojom::ScreenAIAnnotator> receiver_{this};
  const bool create_empty_result_;
  // A negative ID for ui::AXNodeID needs to start from -2 as using -1 for this
  // node id is still incorrectly treated as invalid.
  ui::AXNodeID next_node_id_ = -2;
};

}  // namespace screen_ai::test

#endif  // SERVICES_SCREEN_AI_PUBLIC_TEST_FAKE_SCREEN_AI_ANNOTATOR_H_
