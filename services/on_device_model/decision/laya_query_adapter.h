// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_QUERY_ADAPTER_H_
#define SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_QUERY_ADAPTER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-forward.h"

namespace sentencepiece {
class SentencePieceProcessor;
}  // namespace sentencepiece

namespace on_device_model {

// Converts a `mojom::DecisionQuery` and runtime `input` text into the tensor
// inputs expected by the Laya model. See README.md for the query format.
//
// Each instance is bound to a single `DecisionQuery` and can be reused to
// evaluate multiple inputs against that query.
class LayaQueryAdapter {
 public:
  // Question type IDs matching the model's `qtype` tensor and `temperature`
  // metadata buffer (note: order differs from `mojom::DecisionQuestionType`).
  enum class QuestionType : int32_t {
    kChoice = 0,
    kScore = 1,
    kBoolean = 2,
  };

  // Tokenizes input text, constructed from a serialized SentencePiece
  // tokenizer.
  class Tokenizer {
   public:
    Tokenizer();
    Tokenizer(Tokenizer&&) noexcept;
    Tokenizer& operator=(Tokenizer&&) noexcept;
    ~Tokenizer();

    // Loads the SentencePiece model from `sp_proto_bytes` and resolves the
    // required special token IDs.
    base::expected<void, std::string> Initialize(
        base::span<const uint8_t> sp_proto_bytes);

    bool IsInitialized() const { return processor_ != nullptr; }

    // Encodes `text` into a sequence of SentencePiece token IDs, stripping any
    // reserved special token IDs (`cls`, `sep`, `pad`, `mask`).
    std::vector<int32_t> Tokenize(std::string_view text) const;

    // Start-of-sequence classification token (`[CLS]` or `<bos>`).
    int32_t cls_token_id() const { return cls_token_id_; }
    // Segment separator and end-of-sequence token (`[SEP]` or `<eos>`).
    int32_t sep_token_id() const { return sep_token_id_; }
    // Padding token used to right-pad sequences (`[PAD]` or `<pad>`).
    int32_t pad_token_id() const { return pad_token_id_; }
    // Marker token placed before each candidate option (`[MASK]` or `<mask>`).
    int32_t mask_token_id() const { return mask_token_id_; }

   private:
    std::unique_ptr<sentencepiece::SentencePieceProcessor> processor_;
    int32_t cls_token_id_ = -1;
    int32_t sep_token_id_ = -1;
    int32_t pad_token_id_ = -1;
    int32_t mask_token_id_ = -1;
  };

  struct QuestionInfo {
    QuestionInfo(size_t num_options, std::vector<int32_t> prefix_ids);
    QuestionInfo(QuestionInfo&&) noexcept;
    QuestionInfo& operator=(QuestionInfo&&) noexcept;
    ~QuestionInfo();

    size_t num_options = 0;
    std::vector<int32_t> prefix_ids;
  };

  // Preprocesses `query` into cached token prefixes and option marker metadata.
  static base::expected<LayaQueryAdapter, std::string> Create(
      const mojom::DecisionQuery& query,
      const Tokenizer& tokenizer,
      size_t max_seq_len,
      size_t max_options);

  LayaQueryAdapter(const LayaQueryAdapter&) = delete;
  LayaQueryAdapter& operator=(const LayaQueryAdapter&) = delete;
  LayaQueryAdapter(LayaQueryAdapter&&) noexcept;
  LayaQueryAdapter& operator=(LayaQueryAdapter&&) noexcept;
  ~LayaQueryAdapter();

  // Tokenizes `input` and appends it to each question's preprocessed prefix,
  // returning the unpadded `input_ids` sequence for each question in the batch.
  std::vector<std::vector<int32_t>> ProcessInput(const Tokenizer& tokenizer,
                                                 std::string_view input) const;

  // Flattened `[batch_size, max_options]` indices of each option's `[MASK]`
  // token in `input_ids`.
  base::span<const int32_t> marker_pos() const { return marker_pos_; }

  // Flattened `[batch_size, max_options]` mask (`1` for active options, `0` for
  // unused option slots).
  base::span<const int32_t> marker_mask() const { return marker_mask_; }

  // Per-question type IDs (`[batch_size]`) for the `qtype` model input tensor.
  base::span<const int32_t> question_type() const { return question_type_; }

  // Metadata (`num_options` and `prefix_ids`) for each question in the batch.
  base::span<const QuestionInfo> questions() const { return questions_; }

 private:
  explicit LayaQueryAdapter(size_t max_seq_len);

  size_t max_seq_len_ = 0;

  std::vector<QuestionInfo> questions_;

  std::vector<int32_t> marker_pos_;
  std::vector<int32_t> marker_mask_;
  std::vector<int32_t> question_type_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_DECISION_LAYA_QUERY_ADAPTER_H_
