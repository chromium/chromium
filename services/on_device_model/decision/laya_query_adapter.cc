// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/decision/laya_query_adapter.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/sentencepiece/src/src/sentencepiece_model.pb.h"
#include "third_party/sentencepiece/src/src/sentencepiece_processor.h"

namespace on_device_model {
namespace {

// Returns Laya's trained prompt prefix ("noul" is its boolean type name).
std::string_view GetQuestionTypeName(
    LayaQueryAdapter::QuestionType question_type) {
  switch (question_type) {
    case LayaQueryAdapter::QuestionType::kChoice:
      return "choice";
    case LayaQueryAdapter::QuestionType::kScore:
      return "score";
    case LayaQueryAdapter::QuestionType::kBoolean:
      return "noul";
  }
}

std::string RenderOption(const mojom::DecisionOption& opt,
                         size_t index,
                         LayaQueryAdapter::QuestionType question_type) {
  if (question_type == LayaQueryAdapter::QuestionType::kScore) {
    const std::string& desc =
        opt.description.empty() ? opt.label : opt.description;
    return base::StrCat({"level ", base::NumberToString(index), ": ", desc});
  }
  if (opt.description.empty()) {
    return opt.label;
  }
  return base::StrCat({opt.label, ": ", opt.description});
}

}  // namespace

LayaQueryAdapter::Tokenizer::Tokenizer() = default;
LayaQueryAdapter::Tokenizer::Tokenizer(Tokenizer&&) noexcept = default;
LayaQueryAdapter::Tokenizer& LayaQueryAdapter::Tokenizer::operator=(
    Tokenizer&&) noexcept = default;
LayaQueryAdapter::Tokenizer::~Tokenizer() = default;

base::expected<void, std::string> LayaQueryAdapter::Tokenizer::Initialize(
    base::span<const uint8_t> sp_proto_bytes) {
  auto model_proto = std::make_unique<sentencepiece::ModelProto>();
  if (!model_proto->ParseFromArray(sp_proto_bytes.data(),
                                   sp_proto_bytes.size())) {
    return base::unexpected(
        "Failed to parse embedded sentencepiece::ModelProto.");
  }

  auto sp = std::make_unique<sentencepiece::SentencePieceProcessor>();
  if (auto status = sp->Load(std::move(model_proto)); !status.ok()) {
    return base::unexpected(base::StrCat(
        {"Failed to load SentencePieceProcessor: ", status.message()}));
  }

  // English models use [CLS]/[SEP]/etc.; multilingual models use <bos>/<eos>.
  auto resolve_piece =
      [&sp](const char* a,
            const char* b) -> base::expected<int32_t, std::string> {
    int id = sp->PieceToId(a);
    if (id < 0 || sp->IsUnknown(id)) {
      id = sp->PieceToId(b);
    }
    if (id < 0 || sp->IsUnknown(id)) {
      return base::unexpected(base::StrCat(
          {"SentencePiece model missing required token: ", a, " / ", b}));
    }
    return static_cast<int32_t>(id);
  };

  ASSIGN_OR_RETURN(const int32_t cls_id, resolve_piece("[CLS]", "<bos>"));
  ASSIGN_OR_RETURN(const int32_t sep_id, resolve_piece("[SEP]", "<eos>"));
  ASSIGN_OR_RETURN(const int32_t pad_id, resolve_piece("[PAD]", "<pad>"));
  ASSIGN_OR_RETURN(const int32_t mask_id, resolve_piece("[MASK]", "<mask>"));

  processor_ = std::move(sp);
  cls_token_id_ = cls_id;
  sep_token_id_ = sep_id;
  pad_token_id_ = pad_id;
  mask_token_id_ = mask_id;
  return base::ok();
}

std::vector<int32_t> LayaQueryAdapter::Tokenizer::Tokenize(
    std::string_view text) const {
  if (text.empty() || !processor_) {
    return {};
  }
  std::string sanitized(text);
  for (std::string_view token : {"[CLS]", "<bos>", "[SEP]", "<eos>", "[PAD]",
                                 "<pad>", "[MASK]", "<mask>"}) {
    base::ReplaceSubstringsAfterOffset(&sanitized, 0, token, " ");
  }
  std::vector<int> sp_ids;
  if (!processor_->Encode(sanitized, &sp_ids).ok()) {
    return {};
  }
  std::vector<int32_t> filtered;
  filtered.reserve(sp_ids.size());
  for (int id : sp_ids) {
    if (id != cls_token_id_ && id != sep_token_id_ && id != pad_token_id_ &&
        id != mask_token_id_) {
      filtered.push_back(static_cast<int32_t>(id));
    }
  }
  return filtered;
}

LayaQueryAdapter::QuestionInfo::QuestionInfo(size_t num_options,
                                             std::vector<int32_t> prefix_ids)
    : num_options(num_options), prefix_ids(std::move(prefix_ids)) {}
LayaQueryAdapter::QuestionInfo::QuestionInfo(QuestionInfo&&) noexcept = default;
LayaQueryAdapter::QuestionInfo& LayaQueryAdapter::QuestionInfo::operator=(
    QuestionInfo&&) noexcept = default;
LayaQueryAdapter::QuestionInfo::~QuestionInfo() = default;

// static
base::expected<LayaQueryAdapter, std::string> LayaQueryAdapter::Create(
    const mojom::DecisionQuery& query,
    const Tokenizer& tokenizer,
    size_t max_seq_len,
    size_t max_options) {
  static constexpr size_t kMaxQuestions = 16;
  // Framing tokens in `[CLS] <head> [SEP] [MASK] <opt>... [SEP] <input> [SEP]`.
  static constexpr size_t kNumFramingTokens = 4;
  // Minimum token budget reserved for runtime `<input>` tokens.
  static constexpr size_t kMinInputTokens = 4;
  // Maximum tokens per rendered option (excluding its leading `[MASK]` token).
  static constexpr size_t kMaxOptionTokens = 48;
  // Minimum tokens per option (including its leading `[MASK]` token) when
  // truncating options to fit the sequence budget.
  static constexpr size_t kMinOptionTokens = 4;
  // Preferred minimum token budget for `<head>` before truncating options.
  static constexpr size_t kTargetHeadTokens = 16;
  // Minimum token budget reserved for `<head>`.
  static constexpr size_t kMinHeadTokens = 8;

  if (!tokenizer.IsInitialized()) {
    return base::unexpected("Tokenizer is not initialized.");
  }
  if (max_seq_len <= kNumFramingTokens + kMinInputTokens) {
    return base::unexpected("max_seq_len is too small.");
  }
  if (query.questions.empty()) {
    return base::unexpected(
        "DecisionQuery must contain at least one question.");
  }
  if (query.questions.size() > kMaxQuestions) {
    return base::unexpected("DecisionQuery exceeds maximum question count.");
  }

  LayaQueryAdapter adapter(max_seq_len);
  const size_t head_max_len = max_seq_len - kNumFramingTokens - kMinInputTokens;
  adapter.questions_.reserve(query.questions.size());

  for (const auto& q : query.questions) {
    if (!q) {
      return base::unexpected("DecisionQuery contains a null question.");
    }
    if (q->options.size() < 2u) {
      return base::unexpected(
          "DecisionQuestion must have at least two options.");
    }
    if (q->type == mojom::DecisionQuestionType::kBoolean &&
        q->options.size() != 2u) {
      return base::unexpected(
          "Boolean DecisionQuestion must have exactly two options.");
    }
    // TODO(crbug.com/565849508): `DecisionModelSchemaCompiler` and Blink
    // `ValidateQuestions` allow up to `kMaxChoiceOptions` (26, potentially
    // scaling higher) options, while Laya's `max_options` capacity is 16.
    if (q->options.size() > max_options) {
      return base::unexpected("DecisionQuestion exceeds max_options.");
    }

    QuestionType question_type = QuestionType::kChoice;
    switch (q->type) {
      case mojom::DecisionQuestionType::kChoice:
        question_type = QuestionType::kChoice;
        break;
      case mojom::DecisionQuestionType::kScore:
        question_type = QuestionType::kScore;
        break;
      case mojom::DecisionQuestionType::kBoolean:
        question_type = QuestionType::kBoolean;
        break;
    }

    const std::string head_text = base::StrCat(
        {GetQuestionTypeName(question_type), " question: ", q->prompt});
    std::vector<int32_t> head_ids = tokenizer.Tokenize(head_text);

    std::vector<std::vector<int32_t>> all_option_ids;
    all_option_ids.reserve(q->options.size());
    size_t total_option_tokens = 0;
    for (size_t i = 0; i < q->options.size(); ++i) {
      if (!q->options[i]) {
        return base::unexpected("DecisionQuestion contains a null option.");
      }
      std::vector<int32_t> tokenized = tokenizer.Tokenize(
          base::StrCat({" ", RenderOption(*q->options[i], i, question_type)}));
      if (tokenized.size() > kMaxOptionTokens) {
        tokenized.resize(kMaxOptionTokens);
      }
      std::vector<int32_t> option_ids;
      option_ids.reserve(1 + tokenized.size());
      option_ids.push_back(tokenizer.mask_token_id());
      option_ids.insert(option_ids.end(), tokenized.begin(), tokenized.end());
      total_option_tokens += option_ids.size();
      all_option_ids.push_back(std::move(option_ids));
    }

    // If the tokenized options leave fewer than `kTargetHeadTokens` tokens for
    // `head_ids` within `head_max_len`, uniformly truncate each option (to at
    // least `kMinOptionTokens` tokens) so the question prompt still gets at
    // least `kTargetHeadTokens` (or `kMinHeadTokens` when `head_max_len` is
    // tight) tokens of budget.
    size_t remaining_head_budget = head_max_len > total_option_tokens
                                       ? head_max_len - total_option_tokens
                                       : 0;
    if (remaining_head_budget < kTargetHeadTokens && !all_option_ids.empty()) {
      const size_t min_option_total = all_option_ids.size() * kMinOptionTokens;
      const size_t reserved_head =
          head_max_len >= kTargetHeadTokens + min_option_total
              ? kTargetHeadTokens
              : std::min(head_max_len, kMinHeadTokens);
      const size_t avail = head_max_len - reserved_head;
      const size_t per =
          std::max(kMinOptionTokens, avail / all_option_ids.size());
      total_option_tokens = 0;
      for (auto& o : all_option_ids) {
        if (o.size() > per) {
          o.resize(per);
        }
        total_option_tokens += o.size();
      }
      remaining_head_budget = head_max_len > total_option_tokens
                                  ? head_max_len - total_option_tokens
                                  : 0;
    }

    const size_t required_head_tokens =
        std::min(head_ids.size(), kMinHeadTokens);
    if (remaining_head_budget < required_head_tokens) {
      return base::unexpected(
          "Candidate options exceed max_seq_len sequence budget.");
    }
    // TODO(crbug.com/565849508): Consider whether long prompts or options in
    // `Create()` should also fail with `QuotaExceededError` rather than
    // silently truncating.
    if (head_ids.size() > remaining_head_budget) {
      head_ids.resize(remaining_head_budget);
    }

    // Assemble "[CLS] <head> [SEP] [MASK] <opt>... [SEP]" and record markers.
    std::vector<int32_t> prefix_ids;
    prefix_ids.reserve(3 + head_ids.size() + total_option_tokens);
    prefix_ids.push_back(tokenizer.cls_token_id());
    prefix_ids.insert(prefix_ids.end(), head_ids.begin(), head_ids.end());
    prefix_ids.push_back(tokenizer.sep_token_id());

    std::vector<int32_t> marker_positions(max_options, 0);
    std::vector<int32_t> marker_mask(max_options, 0);
    size_t num_options = 0;

    for (size_t i = 0; i < all_option_ids.size(); ++i) {
      const size_t marker_position = prefix_ids.size();
      if (marker_position + 1 < max_seq_len) {
        marker_positions[i] = base::checked_cast<int32_t>(marker_position);
        marker_mask[i] = 1;
        num_options = i + 1;
      }
      prefix_ids.insert(prefix_ids.end(), all_option_ids[i].begin(),
                        all_option_ids[i].end());
    }
    prefix_ids.push_back(tokenizer.sep_token_id());

    if (num_options != q->options.size() ||
        prefix_ids.size() + 1 > max_seq_len) {
      return base::unexpected(
          "DecisionQuestion options exceed max_seq_len sequence budget.");
    }

    adapter.questions_.emplace_back(num_options, std::move(prefix_ids));
    adapter.marker_pos_.insert(adapter.marker_pos_.end(),
                               marker_positions.begin(),
                               marker_positions.end());
    adapter.marker_mask_.insert(adapter.marker_mask_.end(), marker_mask.begin(),
                                marker_mask.end());
    adapter.question_type_.push_back(std::to_underlying(question_type));
  }

  return adapter;
}

LayaQueryAdapter::LayaQueryAdapter(size_t max_seq_len)
    : max_seq_len_(max_seq_len) {}

LayaQueryAdapter::LayaQueryAdapter(LayaQueryAdapter&&) noexcept = default;
LayaQueryAdapter& LayaQueryAdapter::operator=(LayaQueryAdapter&&) noexcept =
    default;
LayaQueryAdapter::~LayaQueryAdapter() = default;

std::vector<std::vector<int32_t>> LayaQueryAdapter::ProcessInput(
    const Tokenizer& tokenizer,
    std::string_view input) const {
  if (questions_.empty() || !tokenizer.IsInitialized()) {
    return {};
  }

  // TODO(crbug.com/565849508): Cap raw `input` size and report token overflow
  // (`QuotaExceededError`) instead of silently truncating.
  const std::vector<int32_t> input_ids = tokenizer.Tokenize(input);

  std::vector<std::vector<int32_t>> results;
  results.reserve(questions_.size());

  for (const auto& q : questions_) {
    const size_t prefix_len = q.prefix_ids.size();
    const size_t room =
        (max_seq_len_ > prefix_len + 1) ? (max_seq_len_ - prefix_len - 1) : 0;
    const size_t input_count = std::min(input_ids.size(), room);

    std::vector<int32_t> question;
    question.reserve(prefix_len + input_count + 1);
    question.insert(question.end(), q.prefix_ids.begin(), q.prefix_ids.end());
    question.insert(question.end(), input_ids.begin(),
                    input_ids.begin() + input_count);
    question.push_back(tokenizer.sep_token_id());
    results.push_back(std::move(question));
  }

  return results;
}

}  // namespace on_device_model
