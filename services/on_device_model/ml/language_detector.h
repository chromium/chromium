// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_LANGUAGE_DETECTOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_LANGUAGE_DETECTOR_H_

#include <memory>
#include <string_view>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace translate {
class LanguageDetectionModel;
}

namespace ml {

// Wraps a LanguageDetectionModel so it can be shared by multiple objects on
// the same thread.
class LanguageDetector : public base::RefCounted<LanguageDetector> {
 public:
  LanguageDetector(base::PassKey<LanguageDetector>,
                   std::unique_ptr<translate::LanguageDetectionModel> model);

  // Attempts to create a new LanguageDetector using `model_file` as the
  // underlying model. May return null if initialization fails.
  static scoped_refptr<LanguageDetector> Create(base::File model_file);

  // Performs language detection on `text` and returns the result in a mojom
  // wire structure.
  on_device_model::mojom::LanguageDetectionResultPtr DetectLanguage(
      std::string_view text);

 private:
  friend class base::RefCounted<LanguageDetector>;

  ~LanguageDetector();

  const std::unique_ptr<translate::LanguageDetectionModel> model_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_LANGUAGE_DETECTOR_H_
