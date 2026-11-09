// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "services/on_device_model/decision/laya_query_adapter.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-forward.h"

namespace on_device_model {

// Executes a multi-signature Laya `.tflite` model using the LiteRT C++ runtime.
//
// Thread safety: `LayaModel` is thread-safe after construction. Multiple
// `Session` instances created from the same `LayaModel` may be used
// concurrently across threads; calls to `Session::Execute()` are internally
// synchronized with a lock around the shared LiteRT model and tensor buffers.
// Individual `Session` instances are not safe to move or destroy concurrently
// with an in-flight `Execute()` call on the same `Session`.
//
// See README.md for details about the model format.
class LayaModel {
 public:
  class Session {
   public:
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    ~Session();

    // Evaluates `input` against the preprocessed schema and returns the
    // calibrated option probabilities for each question in the query.
    base::expected<std::vector<std::vector<float>>, std::string> Execute(
        std::string_view input);

   private:
    friend class LayaModel;

    Session(LayaModel* model, std::unique_ptr<LayaQueryAdapter> query_adapter);

    raw_ptr<LayaModel> model_ = nullptr;
    std::unique_ptr<LayaQueryAdapter> query_adapter_;
  };

  // Creates a new LayaModel. `model_file` is mapped for the lifetime of the
  // LayaModel. `cache_file` is used for XNNPACK's weight cache; it can be
  // empty, in which case no file-backed caching is performed. If `num_threads`
  // is <= 0, a default thread count based on the system's processor count is
  // used.
  static base::expected<std::unique_ptr<LayaModel>, std::string> Create(
      base::File model_file,
      base::File cache_file = base::File(),
      int num_threads = 0);

  LayaModel(const LayaModel&) = delete;
  LayaModel& operator=(const LayaModel&) = delete;
  ~LayaModel();

  // Preprocesses `query` (question instructions, option texts, and `[MASK]`
  // marker positions/masks) into a reusable `Session`. The `LayaModel` must
  // outlive the returned `Session`.
  base::expected<Session, std::string> CreateSession(
      mojom::DecisionQueryPtr query);

  std::vector<size_t> GetSignatureLengthsForTesting() const;
  float GetCalibrationTemperatureForTesting(
      LayaQueryAdapter::QuestionType question_type) const;

 private:
  class Impl;

  explicit LayaModel(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_MODEL_H_
