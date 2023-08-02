// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/text_classifier/text_classifier_api.h"

namespace ios {
namespace provider {

void SetTextClassifierModelPath(const base::FilePath& model_path) {
  // TextClassifier is not supported for tests.
}

}  // namespace provider
}  // namespace ios
