// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_ML_MODEL_MANAGER_H_
#define SERVICES_AUDIO_TEST_MOCK_ML_MODEL_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "media/webrtc/ml_model_handle.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

// gmock MlModelManager. Deliberately has no default actions: an unconfigured
// GetModel() returns nullptr ("no model"), i.e. a service that has not received
// a model from the browser. Fixtures opt in with ON_CALL.
class MockMlModelManager : public MlModelManager {
 public:
  MockMlModelManager();
  ~MockMlModelManager() override;

  MockMlModelManager(const MockMlModelManager&) = delete;
  MockMlModelManager& operator=(const MockMlModelManager&) = delete;

  MOCK_METHOD(scoped_refptr<media::MlModelHandle>,
              GetModel,
              (mojom::MlModelType model_type),
              (override));
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_ML_MODEL_MANAGER_H_
