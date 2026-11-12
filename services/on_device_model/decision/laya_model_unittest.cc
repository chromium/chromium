// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/decision/laya_model.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/decision/decision_model.h"
#include "services/on_device_model/decision/laya_query_adapter.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/litert/src/tflite/converter/schema/schema_generated.h"
#include "third_party/sentencepiece/src/src/sentencepiece_model.pb.h"

namespace on_device_model {
namespace {

using ::testing::ElementsAre;
using ::testing::FloatNear;

std::string BuildTestSentencePieceProto() {
  sentencepiece::ModelProto model_proto;
  auto* trainer = model_proto.mutable_trainer_spec();
  trainer->set_model_type(sentencepiece::TrainerSpec::UNIGRAM);
  trainer->set_unk_id(3);
  trainer->set_bos_id(2);
  trainer->set_eos_id(1);
  trainer->set_pad_id(0);

  auto* normalizer = model_proto.mutable_normalizer_spec();
  normalizer->set_name("identity");
  normalizer->set_add_dummy_prefix(true);
  normalizer->set_escape_whitespaces(true);

  auto add_piece = [&](const char* text, float score,
                       sentencepiece::ModelProto::SentencePiece::Type type) {
    auto* p = model_proto.add_pieces();
    p->set_piece(text);
    p->set_score(score);
    p->set_type(type);
  };

  // Order pieces 0..3 to match trainer_spec IDs; <mask> is USER_DEFINED.
  using PieceType = sentencepiece::ModelProto::SentencePiece;
  add_piece("<pad>", 0.0f, PieceType::CONTROL);
  add_piece("<eos>", 0.0f, PieceType::CONTROL);
  add_piece("<bos>", 0.0f, PieceType::CONTROL);
  add_piece("<unk>", 0.0f, PieceType::UNKNOWN);
  add_piece("<mask>", 0.0f, PieceType::USER_DEFINED);

  static constexpr auto kWords = std::to_array<const char*>({
      "▁",     "▁choice", "▁score", "▁noul", "▁question", ":",      "▁is",
      "▁good", "▁bad",    "▁yes",   "▁no",   "▁true",     "▁false", "▁level",
      "▁word", "a",       "b",      "c",     "0",         "1",      "2",
  });
  for (const char* w : kWords) {
    add_piece(w, -1.0f, PieceType::NORMAL);
  }
  trainer->set_vocab_size(model_proto.pieces_size());
  return model_proto.SerializeAsString();
}

std::vector<uint8_t> PackFloats(base::span<const float> values) {
  std::vector<uint8_t> out(values.size() * sizeof(float));
  for (size_t i = 0; i < values.size(); ++i) {
    auto bytes = base::U32ToLittleEndian(base::bit_cast<uint32_t>(values[i]));
    base::span(out).subspan(i * sizeof(float), sizeof(float)).copy_from(bytes);
  }
  return out;
}

// Builds a minimal valid multi-signature TFLite FlatBuffer with:
// - `"tokenizer"` and `"temperature"` metadata buffers.
// - Signatures `"seq_32"`, `"seq_64"`, `"seq_128"` where each subgraph outputs
//   `logits = CAST(marker_mask, FLOAT32) * [scale, 0, 0, 0]` with `scale` = 1,
//   2, 3 respectively so tests can observe which signature was selected.
std::vector<uint8_t> BuildMultiSigTestTflite(
    base::span<const float> temperatures) {
  flatbuffers::FlatBufferBuilder fbb(65536);

  std::vector<flatbuffers::Offset<::tflite::Buffer>> buffers;
  // Buffer 0 is always empty in TFLite.
  buffers.push_back(::tflite::CreateBuffer(fbb));

  auto add_raw_buffer = [&](base::span<const uint8_t> bytes) -> uint32_t {
    auto vec = fbb.CreateVector(bytes.data(), bytes.size());
    buffers.push_back(::tflite::CreateBuffer(fbb, vec));
    return static_cast<uint32_t>(buffers.size() - 1);
  };

  // Operator codes: 0 = CAST, 1 = MUL.
  std::vector<flatbuffers::Offset<::tflite::OperatorCode>> op_codes = {
      ::tflite::CreateOperatorCode(fbb, ::tflite::BuiltinOperator_CAST, 0, 1,
                                   ::tflite::BuiltinOperator_CAST),
      ::tflite::CreateOperatorCode(fbb, ::tflite::BuiltinOperator_MUL, 0, 1,
                                   ::tflite::BuiltinOperator_MUL),
  };

  constexpr std::array<int32_t, 3> kSeqLengths = {32, 64, 128};
  constexpr int32_t kMaxOpts = 4;

  std::vector<flatbuffers::Offset<::tflite::SubGraph>> subgraphs;
  std::vector<flatbuffers::Offset<::tflite::SignatureDef>> signatures;

  for (size_t s_idx = 0; s_idx < kSeqLengths.size(); ++s_idx) {
    const int32_t seq_len = kSeqLengths[s_idx];
    const float scale = static_cast<float>(s_idx + 1);
    const std::array<float, kMaxOpts> weights = {2.0f * scale, 0.0f, 0.0f,
                                                 0.0f};
    const uint32_t weight_buf = add_raw_buffer(PackFloats(weights));

    // shape_* sets each tensor's initial batch=1 shape; sig_* sets its
    // shape_signature with -1 so the batch dimension can be resized.
    std::vector<int32_t> shape_seq = {1, seq_len};
    std::vector<int32_t> shape_opts = {1, kMaxOpts};
    std::vector<int32_t> shape_question_type = {1};
    std::vector<int32_t> sig_seq = {-1, seq_len};
    std::vector<int32_t> sig_opts = {-1, kMaxOpts};
    std::vector<int32_t> sig_question_type = {-1};

    std::vector<flatbuffers::Offset<::tflite::Tensor>> tensors = {
        ::tflite::CreateTensorDirect(fbb, &shape_seq,
                                     ::tflite::TensorType_INT32, 0, "input_ids",
                                     /*quantization=*/0, /*is_variable=*/false,
                                     /*sparsity=*/0, &sig_seq),
        ::tflite::CreateTensorDirect(
            fbb, &shape_seq, ::tflite::TensorType_INT32, 0, "attention_mask",
            /*quantization=*/0, /*is_variable=*/false, /*sparsity=*/0,
            &sig_seq),
        ::tflite::CreateTensorDirect(
            fbb, &shape_opts, ::tflite::TensorType_INT32, 0, "marker_pos",
            /*quantization=*/0, /*is_variable=*/false, /*sparsity=*/0,
            &sig_opts),
        ::tflite::CreateTensorDirect(
            fbb, &shape_opts, ::tflite::TensorType_INT32, 0, "marker_mask",
            /*quantization=*/0, /*is_variable=*/false, /*sparsity=*/0,
            &sig_opts),
        ::tflite::CreateTensorDirect(fbb, &shape_question_type,
                                     ::tflite::TensorType_INT32, 0, "qtype",
                                     /*quantization=*/0, /*is_variable=*/false,
                                     /*sparsity=*/0, &sig_question_type),
        ::tflite::CreateTensorDirect(
            fbb, &shape_opts, ::tflite::TensorType_FLOAT32, 0, "mask_f32",
            /*quantization=*/0, /*is_variable=*/false, /*sparsity=*/0,
            &sig_opts),
        ::tflite::CreateTensorDirect(fbb, &shape_opts,
                                     ::tflite::TensorType_FLOAT32, weight_buf,
                                     "weights"),
        ::tflite::CreateTensorDirect(fbb, &shape_opts,
                                     ::tflite::TensorType_FLOAT32, 0, "logits",
                                     /*quantization=*/0, /*is_variable=*/false,
                                     /*sparsity=*/0, &sig_opts),
    };

    // Cast marker_mask (3) to float (5), then multiply by weights (6) -> (7).
    std::vector<int32_t> cast_in = {3};
    std::vector<int32_t> cast_out = {5};
    auto cast_opts = ::tflite::CreateCastOptions(
        fbb, ::tflite::TensorType_INT32, ::tflite::TensorType_FLOAT32);

    std::vector<int32_t> mul_in = {5, 6};
    std::vector<int32_t> mul_out = {7};
    auto mul_opts =
        ::tflite::CreateMulOptions(fbb, ::tflite::ActivationFunctionType_NONE);

    std::vector<flatbuffers::Offset<::tflite::Operator>> ops = {
        ::tflite::CreateOperatorDirect(fbb, 0, &cast_in, &cast_out,
                                       ::tflite::BuiltinOptions_CastOptions,
                                       cast_opts.Union()),
        ::tflite::CreateOperatorDirect(fbb, 1, &mul_in, &mul_out,
                                       ::tflite::BuiltinOptions_MulOptions,
                                       mul_opts.Union()),
    };

    std::vector<int32_t> sg_inputs = {0, 1, 2, 3, 4};
    std::vector<int32_t> sg_outputs = {7};
    const std::string sig_name = "seq_" + base::NumberToString(seq_len);
    subgraphs.push_back(::tflite::CreateSubGraphDirect(
        fbb, &tensors, &sg_inputs, &sg_outputs, &ops, sig_name.c_str()));

    std::vector<flatbuffers::Offset<::tflite::TensorMap>> sig_in_map = {
        ::tflite::CreateTensorMapDirect(fbb, "input_ids", 0),
        ::tflite::CreateTensorMapDirect(fbb, "attention_mask", 1),
        ::tflite::CreateTensorMapDirect(fbb, "marker_pos", 2),
        ::tflite::CreateTensorMapDirect(fbb, "marker_mask", 3),
        ::tflite::CreateTensorMapDirect(fbb, "qtype", 4),
    };
    std::vector<flatbuffers::Offset<::tflite::TensorMap>> sig_out_map = {
        ::tflite::CreateTensorMapDirect(fbb, "logits", 7),
    };
    signatures.push_back(::tflite::CreateSignatureDefDirect(
        fbb, &sig_in_map, &sig_out_map, sig_name.c_str(),
        static_cast<uint32_t>(s_idx)));
  }

  const std::string sp_proto = BuildTestSentencePieceProto();
  const uint32_t tok_buf = add_raw_buffer(base::as_byte_span(sp_proto));
  const uint32_t temp_buf = add_raw_buffer(PackFloats(temperatures));

  std::vector<flatbuffers::Offset<::tflite::Metadata>> metadata = {
      ::tflite::CreateMetadataDirect(fbb, "tokenizer", tok_buf),
      ::tflite::CreateMetadataDirect(fbb, "temperature", temp_buf),
  };

  auto model = ::tflite::CreateModelDirect(
      fbb, 3, &op_codes, &subgraphs, "laya_test", &buffers,
      /*metadata_buffer=*/nullptr, &metadata, &signatures);
  ::tflite::FinishModelBuffer(fbb, model);

  base::span<const uint8_t> buf = fbb.GetBufferSpan();
  return std::vector<uint8_t>(buf.begin(), buf.end());
}

TEST(LayaModelTest, LoadsMetadataAndSelectsSmallestFittingSignature) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath model_path =
      temp_dir.GetPath().AppendASCII("test.tflite");

  constexpr std::array<float, 3> kTemps = {1.5f, 2.0f, 2.5f};

  const std::vector<uint8_t> tflite_bytes = BuildMultiSigTestTflite(kTemps);
  ASSERT_TRUE(base::WriteFile(model_path, tflite_bytes));

  base::File model_file(model_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(model_file.IsValid());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<LayaModel> model,
                       LayaModel::Create(std::move(model_file), base::File(),
                                         /*num_threads=*/2));
  ASSERT_NE(model, nullptr);
  EXPECT_THAT(model->GetSignatureLengthsForTesting(),
              ElementsAre(32u, 64u, 128u));

  // Verify temperature lookups.
  EXPECT_THAT(model->GetCalibrationTemperatureForTesting(
                  LayaQueryAdapter::QuestionType::kChoice),
              FloatNear(1.5f, 1.0e-5f));
  EXPECT_THAT(model->GetCalibrationTemperatureForTesting(
                  LayaQueryAdapter::QuestionType::kScore),
              FloatNear(2.0f, 1.0e-5f));
  EXPECT_THAT(model->GetCalibrationTemperatureForTesting(
                  LayaQueryAdapter::QuestionType::kBoolean),
              FloatNear(2.5f, 1.0e-5f));

  // Build a DecisionQuery with a choice question and a boolean question.
  auto q_choice = mojom::DecisionQuestion::New();
  q_choice->id = "q1";
  q_choice->prompt = "is good";
  q_choice->type = mojom::DecisionQuestionType::kChoice;
  q_choice->options.push_back(mojom::DecisionOption::New("good", "yes"));
  q_choice->options.push_back(mojom::DecisionOption::New("bad", "no"));

  auto q_bool = mojom::DecisionQuestion::New();
  q_bool->id = "q2";
  q_bool->prompt = "is true";
  q_bool->type = mojom::DecisionQuestionType::kBoolean;
  q_bool->options.push_back(mojom::DecisionOption::New("true", "yes"));
  q_bool->options.push_back(mojom::DecisionOption::New("false", "no"));

  auto query = mojom::DecisionQuery::New();
  query->questions.push_back(std::move(q_choice));
  query->questions.push_back(std::move(q_bool));

  {
    ASSERT_OK_AND_ASSIGN(LayaModel::Session session,
                         model->CreateSession(query.Clone()));

    // Short input fits in `seq_32` (scale = 1 -> logits = [2.0, 0.0]).
    ASSERT_OK_AND_ASSIGN(const std::vector<std::vector<float>> short_probs,
                         session.Execute("word"));
    ASSERT_EQ(short_probs.size(), 2u);
    ASSERT_EQ(short_probs[0].size(), 2u);
    ASSERT_EQ(short_probs[1].size(), 2u);

    // Softmax([2.0, 0.0] / T)[0] simplifies to 1 / (1 + exp(-2.0 / T)).
    const float expected_q1_sig32 = 1.0f / (1.0f + std::exp(-2.0f / 1.5f));
    const float expected_q2_sig32 = 1.0f / (1.0f + std::exp(-2.0f / 2.5f));
    EXPECT_THAT(short_probs[0][0], FloatNear(expected_q1_sig32, 1.0e-4f));
    EXPECT_THAT(short_probs[0][1],
                FloatNear(1.0f - expected_q1_sig32, 1.0e-4f));
    EXPECT_THAT(short_probs[1][0], FloatNear(expected_q2_sig32, 1.0e-4f));
    EXPECT_THAT(short_probs[1][1],
                FloatNear(1.0f - expected_q2_sig32, 1.0e-4f));

    // Medium input (> 32 tokens total, <= 64 tokens) selects `seq_64`
    // (scale = 2 -> logits = [4.0, 0.0]).
    std::string medium_input;
    for (int i = 0; i < 25; ++i) {
      medium_input += " word";
    }
    ASSERT_OK_AND_ASSIGN(const std::vector<std::vector<float>> medium_probs,
                         session.Execute(medium_input));
    ASSERT_EQ(medium_probs.size(), 2u);
    const float expected_q1_sig64 = 1.0f / (1.0f + std::exp(-4.0f / 1.5f));
    EXPECT_THAT(medium_probs[0][0], FloatNear(expected_q1_sig64, 1.0e-4f));

    // Long input (> 64 tokens total) selects `seq_128`
    // (scale = 3 -> logits = [6.0, 0.0]).
    std::string long_input;
    for (int i = 0; i < 70; ++i) {
      long_input += " word";
    }
    ASSERT_OK_AND_ASSIGN(const std::vector<std::vector<float>> long_probs,
                         session.Execute(long_input));
    ASSERT_EQ(long_probs.size(), 2u);
    const float expected_q1_sig128 = 1.0f / (1.0f + std::exp(-6.0f / 1.5f));
    EXPECT_THAT(long_probs[0][0], FloatNear(expected_q1_sig128, 1.0e-4f));
  }

  // Verify DecisionModel and DecisionSession delegation.
  DecisionModel decision_model(std::move(model));
  mojo::Remote<mojom::DecisionSession> decision_session;
  decision_model.StartSession(std::move(query),
                              decision_session.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      std::optional<std::vector<mojom::DecisionQuestionResultPtr>>>
      decide_future;
  decision_session->Decide("word", decide_future.GetCallback());
  auto results = decide_future.Take();
  ASSERT_TRUE(results.has_value());
  ASSERT_EQ(results->size(), 2u);
  ASSERT_EQ((*results)[0]->probabilities.size(), 2u);
  const float expected_q1_sig32 = 1.0f / (1.0f + std::exp(-2.0f / 1.5f));
  const float expected_q2_sig32 = 1.0f / (1.0f + std::exp(-2.0f / 2.5f));
  EXPECT_THAT((*results)[0]->probabilities[0],
              FloatNear(expected_q1_sig32, 1.0e-4f));
  EXPECT_THAT((*results)[1]->probabilities[0],
              FloatNear(expected_q2_sig32, 1.0e-4f));

  // Verify that queries with < 2 options or > max_options options reset the
  // session pipe with a disconnect reason.
  {
    auto one_opt_query = mojom::DecisionQuery::New();
    auto q_one = mojom::DecisionQuestion::New();
    q_one->id = "one";
    q_one->prompt = "is good";
    q_one->type = mojom::DecisionQuestionType::kChoice;
    q_one->options.push_back(mojom::DecisionOption::New("good", "yes"));
    one_opt_query->questions.push_back(std::move(q_one));

    mojo::Remote<mojom::DecisionSession> one_opt_session;
    decision_model.StartSession(std::move(one_opt_query),
                                one_opt_session.BindNewPipeAndPassReceiver());
    base::test::TestFuture<uint32_t, const std::string&> disconnect_future;
    one_opt_session.set_disconnect_with_reason_handler(
        disconnect_future.GetCallback());
    EXPECT_FALSE(std::get<1>(disconnect_future.Get()).empty());
  }
  {
    auto too_many_opts_query = mojom::DecisionQuery::New();
    auto q_many = mojom::DecisionQuestion::New();
    q_many->id = "many";
    q_many->prompt = "is good";
    q_many->type = mojom::DecisionQuestionType::kChoice;
    for (int i = 0; i < 5; ++i) {
      q_many->options.push_back(mojom::DecisionOption::New("good", "yes"));
    }
    too_many_opts_query->questions.push_back(std::move(q_many));

    mojo::Remote<mojom::DecisionSession> many_session;
    decision_model.StartSession(std::move(too_many_opts_query),
                                many_session.BindNewPipeAndPassReceiver());
    base::test::TestFuture<uint32_t, const std::string&> disconnect_future;
    many_session.set_disconnect_with_reason_handler(
        disconnect_future.GetCallback());
    EXPECT_FALSE(std::get<1>(disconnect_future.Get()).empty());
  }
}

TEST(LayaQueryAdapterTest, SanitizesMasksAndCapsPrefixLength) {
  const std::string sp_proto = BuildTestSentencePieceProto();
  LayaQueryAdapter::Tokenizer tokenizer;
  ASSERT_OK(tokenizer.Initialize(base::as_byte_span(sp_proto)));

  // Score question with literal "[MASK]", "<mask>", and control tokens in
  // prompt, options, and input.
  auto q_score = mojom::DecisionQuestion::New();
  q_score->id = "s1";
  q_score->prompt = "is [MASK] good <mask> <bos> <eos> <pad>";
  q_score->type = mojom::DecisionQuestionType::kScore;
  q_score->options.push_back(mojom::DecisionOption::New("0", "bad [MASK]"));
  q_score->options.push_back(mojom::DecisionOption::New("1", "good <mask>"));

  auto query = mojom::DecisionQuery::New();
  query->questions.push_back(std::move(q_score));

  ASSERT_OK_AND_ASSIGN(
      LayaQueryAdapter adapter,
      LayaQueryAdapter::Create(*query, tokenizer, /*max_seq_len=*/64,
                               /*max_options=*/4));
  ASSERT_EQ(adapter.questions().size(), 1u);
  EXPECT_EQ(adapter.question_type()[0],
            std::to_underlying(LayaQueryAdapter::QuestionType::kScore));
  EXPECT_EQ(adapter.questions()[0].num_options, 2u);

  const std::vector<std::vector<int32_t>> seqs =
      adapter.ProcessInput(tokenizer, "word [MASK] <mask> <bos> <eos> <pad>");
  ASSERT_EQ(seqs.size(), 1u);
  const auto& seq = seqs[0];
  EXPECT_LE(seq.size(), 64u);
  EXPECT_EQ(seq.front(), tokenizer.cls_token_id());
  EXPECT_EQ(seq.back(), tokenizer.sep_token_id());

  // Exactly 2 mask tokens should appear in `seq`, at the recorded `marker_pos`
  // indices, and no extra `[CLS]`, `[SEP]`, or `[PAD]` tokens should appear
  // outside the framing boundaries.
  size_t mask_count = 0;
  size_t cls_count = 0;
  size_t sep_count = 0;
  for (int32_t id : seq) {
    if (id == tokenizer.mask_token_id()) {
      ++mask_count;
    } else if (id == tokenizer.cls_token_id()) {
      ++cls_count;
    } else if (id == tokenizer.sep_token_id()) {
      ++sep_count;
    }
    EXPECT_NE(id, tokenizer.pad_token_id());
  }
  EXPECT_EQ(mask_count, 2u);
  EXPECT_EQ(cls_count, 1u);
  EXPECT_EQ(sep_count, 3u);
  EXPECT_EQ(adapter.marker_mask()[0], 1);
  EXPECT_EQ(adapter.marker_mask()[1], 1);
  EXPECT_EQ(adapter.marker_mask()[2], 0);
  EXPECT_EQ(adapter.marker_mask()[3], 0);
  EXPECT_EQ(seq[adapter.marker_pos()[0]], tokenizer.mask_token_id());
  EXPECT_EQ(seq[adapter.marker_pos()[1]], tokenizer.mask_token_id());

  // When `max_seq_len` is too small to fit framing + minimum header + all
  // options + minimum `<input>` reserve, `Create()` must fail rather than
  // silently dropping options.
  EXPECT_FALSE(LayaQueryAdapter::Create(*query, tokenizer, /*max_seq_len=*/16,
                                        /*max_options=*/4)
                   .has_value());

  // Verify tight `max_seq_len` capping: `Create()` preserves both options and
  // leaves budget for `<input>`, and `ProcessInput()` output never exceeds
  // `max_seq_len`, ends with `[SEP]`, preserves `<input>` tokens before the
  // closing `[SEP]`, and keeps every active `marker_pos` pointing to `[MASK]`.
  ASSERT_OK_AND_ASSIGN(
      LayaQueryAdapter tight_adapter,
      LayaQueryAdapter::Create(*query, tokenizer, /*max_seq_len=*/24,
                               /*max_options=*/4));
  EXPECT_EQ(tight_adapter.questions()[0].num_options, 2u);
  const std::vector<std::vector<int32_t>> tight_seqs =
      tight_adapter.ProcessInput(tokenizer,
                                 "word word word word word word word");
  ASSERT_EQ(tight_seqs.size(), 1u);
  EXPECT_EQ(tight_seqs[0].size(), 24u);
  EXPECT_EQ(tight_seqs[0].back(), tokenizer.sep_token_id());
  EXPECT_GT(tight_seqs[0].size(),
            tight_adapter.questions()[0].prefix_ids.size() + 1u);
  for (size_t i = 0; i < 4; ++i) {
    if (tight_adapter.marker_mask()[i] != 0) {
      const size_t pos = static_cast<size_t>(tight_adapter.marker_pos()[i]);
      ASSERT_LT(pos, tight_seqs[0].size());
      EXPECT_EQ(tight_seqs[0][pos], tokenizer.mask_token_id());
    }
  }
}

}  // namespace
}  // namespace on_device_model
