// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_CLASSIFIER_TEXT_CLASSIFIER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_CLASSIFIER_TEXT_CLASSIFIER_API_H_

#import "base/files/file_path.h"

namespace ios {
namespace provider {

// Sets a static model path to be used by ML TextClassifier instances.
void SetTextClassifierModelPath(const base::FilePath& model_path);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_CLASSIFIER_TEXT_CLASSIFIER_API_H_
