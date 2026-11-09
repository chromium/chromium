// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/decision/laya_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/on_device_model/decision/laya_query_adapter.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "third_party/flatbuffers/src/include/flatbuffers/verifier.h"
#include "third_party/litert/buildflags.h"
#include "third_party/litert/src/litert/c/litert_common.h"
#include "third_party/litert/src/litert/cc/internal/scoped_file.h"
#include "third_party/litert/src/litert/cc/litert_buffer_ref.h"
#include "third_party/litert/src/litert/cc/litert_compiled_model.h"
#include "third_party/litert/src/litert/cc/litert_element_type.h"
#include "third_party/litert/src/litert/cc/litert_environment.h"
#include "third_party/litert/src/litert/cc/litert_expected.h"
#include "third_party/litert/src/litert/cc/litert_layout.h"
#include "third_party/litert/src/litert/cc/litert_model.h"
#include "third_party/litert/src/litert/cc/litert_options.h"
#include "third_party/litert/src/litert/cc/litert_ranked_tensor_type.h"
#include "third_party/litert/src/litert/cc/litert_tensor_buffer.h"
#include "third_party/litert/src/litert/cc/options/litert_cpu_options.h"
#include "third_party/litert/src/tflite/converter/schema/schema_generated.h"

#if BUILDFLAG(BUILD_LITERT_WITH_XNNPACK)
#include "third_party/litert/src/tflite/delegates/xnnpack/file_util.h"
#include "third_party/litert/src/tflite/delegates/xnnpack/xnnpack_delegate.h"
#endif

namespace on_device_model {
namespace {

// Scales a question's active logits by temperature and computes softmax in
// C++, since the graph outputs raw logits for all max_options slots.
std::vector<float> ComputeSoftmaxWithTemperature(base::span<const float> logits,
                                                 float temperature) {
  if (logits.empty()) {
    return {};
  }
  const float inv_temp = 1.0f / temperature;
  float max_z = -std::numeric_limits<float>::infinity();
  for (float v : logits) {
    max_z = std::max(max_z, v * inv_temp);
  }

  std::vector<float> probs(logits.size(), 0.0f);
  float sum_exp = 0.0f;
  for (size_t i = 0; i < logits.size(); ++i) {
    const float e = std::exp((logits[i] * inv_temp) - max_z);
    probs[i] = e;
    sum_exp += e;
  }
  if (std::isfinite(sum_exp) && sum_exp > 0.0f) {
    const float inv_sum = 1.0f / sum_exp;
    for (float& p : probs) {
      p *= inv_sum;
    }
  } else {
    std::ranges::fill(probs, 0.0f);
  }
  return probs;
}

// Looks up a named buffer in the `.tflite` FlatBuffer `metadata` table.
base::span<const uint8_t> FindNamedBuffer(const ::tflite::Model* model,
                                          std::string_view name) {
  if (!model || !model->metadata() || !model->buffers()) {
    return {};
  }
  for (const ::tflite::Metadata* meta : *model->metadata()) {
    if (!meta || !meta->name() || meta->name()->string_view() != name) {
      continue;
    }
    const uint32_t buf_idx = meta->buffer();
    if (buf_idx >= model->buffers()->size()) {
      continue;
    }
    const ::tflite::Buffer* buf = model->buffers()->Get(buf_idx);
    if (buf && buf->data() && buf->data()->size() > 0) {
      return flatbuffers::make_span(buf->data());
    }
  }
  return {};
}

std::optional<size_t> FindTensorNameIndex(
    base::span<const std::string_view> names,
    std::string_view target) {
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] == target) {
      return i;
    }
  }
  return std::nullopt;
}

}  // namespace

class LayaModel::Impl {
 public:
  Impl() = default;
  ~Impl() = default;

  // Memory-maps and validates `model_file`, loads its embedded tokenizer and
  // calibration temperatures, and compiles the model for LiteRT inference.
  base::expected<void, std::string> Initialize(base::File model_file,
                                               base::File cache_file,
                                               int num_threads) {
    if (!model_file.IsValid() ||
        !mapped_file_.Initialize(std::move(model_file))) {
      return base::unexpected("Failed to memory-map Laya model.");
    }

    base::span<const uint8_t> model_bytes = mapped_file_.bytes();
    flatbuffers::Verifier verifier(model_bytes.data(), model_bytes.size());
    if (!::tflite::VerifyModelBuffer(verifier)) {
      return base::unexpected("Invalid TFLite FlatBuffer in Laya model.");
    }

    const ::tflite::Model* raw_model = ::tflite::GetModel(model_bytes.data());
    if (!raw_model) {
      return base::unexpected("Failed to read TFLite Model from FlatBuffer.");
    }

    base::span<const uint8_t> tokenizer_bytes =
        FindNamedBuffer(raw_model, "tokenizer");
    if (tokenizer_bytes.empty()) {
      return base::unexpected("Laya model missing 'tokenizer' buffer.");
    }
    RETURN_IF_ERROR(tokenizer_.Initialize(tokenizer_bytes));
    RETURN_IF_ERROR(ParseTemperatureBuffer(raw_model));

    auto env = ::litert::Environment::Create({});
    if (!env) {
      return base::unexpected(base::StrCat(
          {"Failed to create LiteRT Environment: ", env.Error().Message()}));
    }
    env_.emplace(std::move(*env));

    return InitCompiledModelAndSignatures(model_bytes, std::move(cache_file),
                                          num_threads);
  }

  std::vector<size_t> GetSignatureLengths() const {
    std::vector<size_t> lengths;
    lengths.reserve(signatures_.size());
    for (const auto& sig : signatures_) {
      lengths.push_back(sig.seq_len);
    }
    return lengths;
  }

  float GetCalibrationTemperature(
      LayaQueryAdapter::QuestionType question_type) const {
    const size_t idx = static_cast<size_t>(
        std::clamp<int32_t>(std::to_underlying(question_type), 0, 2));
    return std::max(temperatures_[idx], 1.0e-3f);
  }

  base::expected<std::unique_ptr<LayaQueryAdapter>, std::string>
  CreateQueryAdapter(const mojom::DecisionQuery& query) const {
    ASSIGN_OR_RETURN(LayaQueryAdapter adapter,
                     LayaQueryAdapter::Create(query, tokenizer_, max_seq_len_,
                                              max_options_));
    return std::make_unique<LayaQueryAdapter>(std::move(adapter));
  }

  base::expected<std::vector<std::vector<float>>, std::string> Execute(
      const LayaQueryAdapter& adapter,
      std::string_view input) {
    if (adapter.questions().empty() || signatures_.empty() ||
        !compiled_model_) {
      return base::unexpected(
          "LayaModel or LayaQueryAdapter is not initialized.");
    }

    // Create the 2D input tensor:
    //   <------------- signature sequence length ------------>
    //   +------------+-------------------+-------------------+   ^
    //   |   prefix   |      input 0      |      padding      |   |
    //   +------------+---------+---------+-------------------+ batch
    //   |   prefix   | input 1 |           padding           |  size
    //   +------------+---------+-------------+---------------+   |
    //   |   prefix   |        input 2        |    padding    |   v
    //   +------------+-----------------------+---------------+
    std::vector<std::vector<int32_t>> question_inputs =
        adapter.ProcessInput(tokenizer_, input);
    if (question_inputs.size() != adapter.questions().size()) {
      return base::unexpected("Failed to process input for query questions.");
    }
    size_t max_question_len = 0;
    for (const auto& question_ids : question_inputs) {
      max_question_len = std::max(max_question_len, question_ids.size());
    }

    base::AutoLock lock(lock_);

    SignatureEntry& sig = SelectSignature(max_question_len);
    const size_t sig_seq_len = sig.seq_len;
    const size_t batch_size = question_inputs.size();
    const size_t total_seq_tokens =
        base::CheckMul(batch_size, sig_seq_len).ValueOrDie();

    std::vector<int32_t> input_ids(total_seq_tokens, tokenizer_.pad_token_id());
    std::vector<int32_t> attention_mask(total_seq_tokens, 0);

    for (size_t b = 0; b < batch_size; ++b) {
      base::span<const int32_t> question_ids = question_inputs[b];
      const size_t to_copy = std::min(question_ids.size(), sig_seq_len);
      base::span(input_ids)
          .subspan(b * sig_seq_len, to_copy)
          .copy_from(question_ids.first(to_copy));
      std::ranges::fill(
          base::span(attention_mask).subspan(b * sig_seq_len, to_copy), 1);
    }

    ASSIGN_OR_RETURN(
        std::vector<float> raw_logits,
        RunSignatureLocked(sig, batch_size, input_ids, attention_mask,
                           adapter.marker_pos(), adapter.marker_mask(),
                           adapter.question_type()));

    std::vector<std::vector<float>> results;
    results.reserve(batch_size);
    for (size_t b = 0; b < batch_size; ++b) {
      const auto& q = adapter.questions()[b];
      // Slice only active option logits so unused slots don't affect softmax.
      base::span<const float> row_logits =
          base::span(raw_logits)
              .subspan(b * sig.max_options,
                       std::min(q.num_options, sig.max_options));
      const auto question_type = static_cast<LayaQueryAdapter::QuestionType>(
          adapter.question_type()[b]);
      results.push_back(ComputeSoftmaxWithTemperature(
          row_logits, GetCalibrationTemperature(question_type)));
    }

    return results;
  }

 private:
  struct SignatureEntry {
    // The index of this signature in the model.
    size_t sig_index = SIZE_MAX;
    // The sequence length that this signature uses.
    size_t seq_len = 0;
    // The maximum number of options that this signature supports.
    size_t max_options = 0;

    // The indices of the input tensors for this signature.
    size_t idx_input_ids = SIZE_MAX;
    size_t idx_attention_mask = SIZE_MAX;
    size_t idx_marker_pos = SIZE_MAX;
    size_t idx_marker_mask = SIZE_MAX;
    size_t idx_question_type = SIZE_MAX;
    // The index of the output tensor for this signature.
    size_t idx_logits = SIZE_MAX;

    // Cached input/output buffers for this signature, and the batch size it was
    // last run with. If the signature is executed again with the same
    // `batch_size`, then the cached buffers are reused.
    size_t batch_size = 0;
    std::vector<::litert::TensorBuffer> input_buffers;
    std::vector<::litert::TensorBuffer> output_buffers;
  };

  // Loads optional per-question-type temperatures, keeping 1.0f if absent.
  base::expected<void, std::string> ParseTemperatureBuffer(
      const ::tflite::Model* raw_model) {
    base::span<const uint8_t> temp_bytes =
        FindNamedBuffer(raw_model, "temperature");
    if (temp_bytes.empty()) {
      return base::ok();
    }
    if (temp_bytes.size() != temperatures_.size() * sizeof(float)) {
      return base::unexpected(
          base::StrCat({"Unexpected 'temperature' buffer size: ",
                        base::NumberToString(temp_bytes.size())}));
    }
    for (size_t i = 0; i < temperatures_.size(); ++i) {
      const float temp = base::FloatFromLittleEndian(
          temp_bytes.subspan(i * sizeof(float)).first<sizeof(float)>());
      if (std::isfinite(temp) && temp > 0.0f) {
        temperatures_[i] = temp;
      }
    }
    return base::ok();
  }

  // Compiles the CPU model and caches validated seq_N signatures by length.
  base::expected<void, std::string> InitCompiledModelAndSignatures(
      base::span<const uint8_t> bytes,
      base::File cache_file,
      int num_threads) {
    auto options = ::litert::Options::Create();
    if (!options) {
      return base::unexpected(base::StrCat(
          {"LiteRT Options::Create failed: ", options.Error().Message()}));
    }
    options->SetHardwareAccelerators(::litert::HwAccelerators::kCpu);
#if BUILDFLAG(BUILD_LITERT_WITH_XNNPACK)
    ::tflite::xnnpack::FileDescriptor cache_fd;
#endif
    if (auto cpu_opts = options->GetCpuOptions()) {
      const int effective_threads =
          num_threads > 0
              ? num_threads
              : std::min(4, (base::SysInfo::NumberOfProcessors() + 1) / 2);
      cpu_opts->SetNumThreads(effective_threads);
#if BUILDFLAG(BUILD_LITERT_WITH_XNNPACK)
      cpu_opts->SetXNNPackFlags(
          TfLiteXNNPackDelegateOptionsDefault().flags |
          TFLITE_XNNPACK_DELEGATE_FLAG_DYNAMIC_FULLY_CONNECTED |
          TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_LATEST_OPERATORS);
      if (cache_file.IsValid()) {
        ::litert::ScopedFile scoped_cache_file(cache_file.TakePlatformFile());
        if (auto fd = scoped_cache_file.Release(); fd.ok()) {
          cache_fd.Reset(*fd);
          if (*fd < 0 || !cpu_opts->SetXNNPackWeightCacheFileDescriptor(*fd)) {
            cache_fd.Close();
          }
        }
      } else if (TfLiteXNNPackDelegateCanUseInMemoryWeightCacheProvider()) {
        // TODO(crbug.com/565849508): Check if we want to use the in-memory
        // cache.
        cpu_opts->SetXNNPackWeightCachePath(
            TfLiteXNNPackDelegateInMemoryFilePath());
      }
#endif
    }

    ::litert::BufferRef<uint8_t> model_ref(bytes.data(), bytes.size());
    auto compiled = ::litert::CompiledModel::Create(*env_, model_ref, *options);
    if (!compiled) {
      return base::unexpected(
          base::StrCat({"LiteRT CompiledModel::Create failed: ",
                        compiled.Error().Message()}));
    }
#if BUILDFLAG(BUILD_LITERT_WITH_XNNPACK)
    // XNNPACK took ownership of `cache_fd` during `CompiledModel::Create()`.
    cache_fd.Release();
#endif
    compiled_model_.emplace(std::move(*compiled));

    const size_t num_sigs = compiled_model_->GetNumSignatures();
    // Find all "seq_<N>" signatures and validate their input/output tensors.
    for (size_t sig_idx = 0; sig_idx < num_sigs; ++sig_idx) {
      auto sig = compiled_model_->GetSignature(sig_idx);
      if (!sig) {
        continue;
      }
      const std::string_view sig_key = sig->Key();
      if (!base::StartsWith(sig_key, "seq_")) {
        continue;
      }
      size_t seq_len = 0;
      if (!base::StringToSizeT(sig_key.substr(4), &seq_len) || seq_len < 2 ||
          !base::IsValueInRangeForNumericType<int32_t>(seq_len)) {
        continue;
      }

      auto in_names = compiled_model_->GetSignatureInputNames(sig_idx);
      auto out_names = compiled_model_->GetSignatureOutputNames(sig_idx);
      if (!in_names || !out_names || in_names->size() != 5u ||
          out_names->size() != 1u) {
        return base::unexpected(base::StrCat(
            {"Unexpected input/output tensor count for signature ", sig_key}));
      }

      const std::optional<size_t> idx_input_ids =
          FindTensorNameIndex(*in_names, "input_ids");
      const std::optional<size_t> idx_attention_mask =
          FindTensorNameIndex(*in_names, "attention_mask");
      const std::optional<size_t> idx_marker_pos =
          FindTensorNameIndex(*in_names, "marker_pos");
      const std::optional<size_t> idx_marker_mask =
          FindTensorNameIndex(*in_names, "marker_mask");
      const std::optional<size_t> idx_question_type =
          FindTensorNameIndex(*in_names, "qtype");
      const std::optional<size_t> idx_logits =
          FindTensorNameIndex(*out_names, "logits");
      if (!idx_input_ids || !idx_attention_mask || !idx_marker_pos ||
          !idx_marker_mask || !idx_question_type || !idx_logits) {
        return base::unexpected(
            base::StrCat({"Signature ", sig_key,
                          " missing required input/output tensors."}));
      }

      SignatureEntry entry = {
          .sig_index = sig_idx,
          .seq_len = seq_len,
          .idx_input_ids = *idx_input_ids,
          .idx_attention_mask = *idx_attention_mask,
          .idx_marker_pos = *idx_marker_pos,
          .idx_marker_mask = *idx_marker_mask,
          .idx_question_type = *idx_question_type,
          .idx_logits = *idx_logits,
      };

      auto check_int32_2d =
          [&](size_t input_idx,
              std::optional<int32_t> expected_dim1 =
                  std::nullopt) -> base::expected<int32_t, std::string> {
        auto t = compiled_model_->GetInputTensorType(sig_idx, input_idx);
        if (!t || t->ElementType() != ::litert::ElementType::Int32) {
          return base::unexpected(base::StrCat(
              {"Signature ", sig_key, " input ",
               base::NumberToString(input_idx), " is not an INT32 tensor."}));
        }
        auto dims = t->Layout().Dimensions();
        if (dims.size() != 2u || (dims[0] != 1 && dims[0] != -1) ||
            dims[1] <= 0 || (expected_dim1 && dims[1] != *expected_dim1)) {
          return base::unexpected(
              base::StrCat({"Signature ", sig_key, " input ",
                            base::NumberToString(input_idx),
                            " has unexpected 2D tensor shape."}));
        }
        return dims[1];
      };

      // Validate `marker_pos` input tensor's shape
      // (`[batch_size, max_options]`) and extract `max_options` from its
      // second dimension.
      ASSIGN_OR_RETURN(const int32_t max_opts_dim,
                       check_int32_2d(entry.idx_marker_pos));
      entry.max_options = static_cast<size_t>(max_opts_dim);
      // Every seq_N signature must share the same fixed max_options capacity,
      // since LayaQueryAdapter builds marker tensors before seq_N is chosen.
      if (!signatures_.empty() &&
          entry.max_options != signatures_.front().max_options) {
        return base::unexpected("Mismatched max_options across signatures.");
      }

      const int32_t seq_len_dim = base::checked_cast<int32_t>(seq_len);
      RETURN_IF_ERROR(check_int32_2d(entry.idx_input_ids, seq_len_dim));
      RETURN_IF_ERROR(check_int32_2d(entry.idx_attention_mask, seq_len_dim));
      RETURN_IF_ERROR(check_int32_2d(entry.idx_marker_mask, max_opts_dim));

      auto question_type_type =
          compiled_model_->GetInputTensorType(sig_idx, entry.idx_question_type);
      if (!question_type_type ||
          question_type_type->ElementType() != ::litert::ElementType::Int32) {
        return base::unexpected(base::StrCat(
            {"Signature ", sig_key, " has invalid qtype tensor type."}));
      }
      auto question_type_dims = question_type_type->Layout().Dimensions();
      if (question_type_dims.size() != 1u ||
          (question_type_dims[0] != 1 && question_type_dims[0] != -1)) {
        return base::unexpected(base::StrCat(
            {"Signature ", sig_key, " has invalid qtype dimensions."}));
      }

      auto logits_type =
          compiled_model_->GetOutputTensorType(sig_idx, entry.idx_logits);
      if (!logits_type ||
          logits_type->ElementType() != ::litert::ElementType::Float32) {
        return base::unexpected(base::StrCat(
            {"Signature ", sig_key, " has invalid logits tensor type."}));
      }
      auto logits_dims = logits_type->Layout().Dimensions();
      if (logits_dims.size() != 2u ||
          (logits_dims[0] != 1 && logits_dims[0] != -1) ||
          logits_dims[1] != max_opts_dim) {
        return base::unexpected(base::StrCat(
            {"Signature ", sig_key, " has invalid logits dimensions."}));
      }

      signatures_.push_back(std::move(entry));
    }

    if (signatures_.empty()) {
      return base::unexpected(
          "LayaModel found no 'seq_N' entrypoint signatures.");
    }

    std::ranges::sort(signatures_, {}, &SignatureEntry::seq_len);
    max_seq_len_ = signatures_.back().seq_len;
    max_options_ = signatures_.back().max_options;
    return base::ok();
  }

  // Selects the smallest signature whose `seq_len >= required_len`, or falls
  // back to the largest signature if `required_len` exceeds all signatures.
  SignatureEntry& SelectSignature(size_t required_len) {
    for (auto& sig : signatures_) {
      if (sig.seq_len >= required_len) {
        return sig;
      }
    }
    return signatures_.back();
  }

  // Resizes batch dim via NonStrict (signatures may declare dim 1, not -1).
  base::expected<void, std::string> ResizeInputTensorLocked(
      size_t sig_index,
      size_t input_index,
      base::span<const int> dims) {
    auto res = compiled_model_->ResizeInputTensorNonStrict(
        sig_index, input_index, absl::MakeConstSpan(dims.data(), dims.size()));
    if (!res) {
      return base::unexpected(base::StrCat(
          {"LiteRT ResizeInputTensorNonStrict failed on signature ",
           base::NumberToString(sig_index), " input ",
           base::NumberToString(input_index), ": ", res.Error().Message()}));
    }
    return base::ok();
  }

  // Allocates or reuses buffers for sig, runs inference, and returns logits.
  base::expected<std::vector<float>, std::string> RunSignatureLocked(
      SignatureEntry& sig,
      size_t batch_size,
      base::span<const int32_t> input_ids,
      base::span<const int32_t> attention_mask,
      base::span<const int32_t> marker_pos,
      base::span<const int32_t> marker_mask,
      base::span<const int32_t> question_type) {
    // If the signature doesn't already have buffers that are the right size,
    // then this is either the first time we've executed the model, or a
    // different signature or batch size was previously used. In either case,
    // release any previously used buffers and recreate new ones that are the
    // right size.
    if (sig.input_buffers.empty() || sig.output_buffers.empty() ||
        sig.batch_size != batch_size) {
      for (auto& entry : signatures_) {
        entry.input_buffers.clear();
        entry.output_buffers.clear();
        entry.batch_size = 0;
      }

      // Resize the signature's input tensors to match the desired batch size.
      const int batch_dim = base::checked_cast<int>(batch_size);
      const std::array<int, 2> seq_dims = {
          batch_dim, base::checked_cast<int>(sig.seq_len)};
      const std::array<int, 2> opt_dims = {
          batch_dim, base::checked_cast<int>(sig.max_options)};
      const std::array<int, 1> question_type_dims = {batch_dim};
      RETURN_IF_ERROR(
          ResizeInputTensorLocked(sig.sig_index, sig.idx_input_ids, seq_dims));
      RETURN_IF_ERROR(ResizeInputTensorLocked(
          sig.sig_index, sig.idx_attention_mask, seq_dims));
      RETURN_IF_ERROR(
          ResizeInputTensorLocked(sig.sig_index, sig.idx_marker_pos, opt_dims));
      RETURN_IF_ERROR(ResizeInputTensorLocked(sig.sig_index,
                                              sig.idx_marker_mask, opt_dims));
      RETURN_IF_ERROR(ResizeInputTensorLocked(
          sig.sig_index, sig.idx_question_type, question_type_dims));

      auto in_bufs = compiled_model_->CreateInputBuffers(sig.sig_index);
      auto out_bufs = compiled_model_->CreateOutputBuffers(sig.sig_index);
      if (!in_bufs || !out_bufs) {
        return base::unexpected(
            base::StrCat({"Failed to create buffers for signature seq_",
                          base::NumberToString(sig.seq_len)}));
      }
      sig.input_buffers = std::move(*in_bufs);
      sig.output_buffers = std::move(*out_bufs);
      sig.batch_size = batch_size;
    }

    if (!sig.input_buffers[sig.idx_input_ids].Write<int32_t>(
            absl::MakeConstSpan(input_ids.data(), input_ids.size())) ||
        !sig.input_buffers[sig.idx_attention_mask].Write<int32_t>(
            absl::MakeConstSpan(attention_mask.data(),
                                attention_mask.size())) ||
        !sig.input_buffers[sig.idx_marker_pos].Write<int32_t>(
            absl::MakeConstSpan(marker_pos.data(), marker_pos.size())) ||
        !sig.input_buffers[sig.idx_marker_mask].Write<int32_t>(
            absl::MakeConstSpan(marker_mask.data(), marker_mask.size())) ||
        !sig.input_buffers[sig.idx_question_type].Write<int32_t>(
            absl::MakeConstSpan(question_type.data(), question_type.size()))) {
      return base::unexpected(
          base::StrCat({"Failed to write input buffers for signature seq_",
                        base::NumberToString(sig.seq_len)}));
    }

    auto run_res = compiled_model_->Run(sig.sig_index, sig.input_buffers,
                                        sig.output_buffers);
    if (!run_res) {
      return base::unexpected(
          base::StrCat({"LiteRT CompiledModel::Run failed on seq_",
                        base::NumberToString(sig.seq_len), ": ",
                        run_res.Error().Message()}));
    }

    const size_t num_logits =
        base::CheckMul(batch_size, sig.max_options).ValueOrDie();
    std::vector<float> out_logits(num_logits, 0.0f);
    if (!sig.output_buffers[sig.idx_logits].Read<float>(
            absl::MakeSpan(out_logits.data(), out_logits.size()))) {
      return base::unexpected(
          base::StrCat({"Failed to read output logits for signature seq_",
                        base::NumberToString(sig.seq_len)}));
    }
    return out_logits;
  }

  base::MemoryMappedFile mapped_file_;
  std::optional<::litert::Environment> env_;
  std::optional<::litert::CompiledModel> compiled_model_;
  std::vector<SignatureEntry> signatures_;

  // The sequence length of the longest signature in the model.
  size_t max_seq_len_ = 0;
  // The fixed option capacity shared by all `seq_N` signatures in the model.
  size_t max_options_ = 0;

  LayaQueryAdapter::Tokenizer tokenizer_;

  std::array<float, 3> temperatures_ = {1.0f, 1.0f, 1.0f};

  base::Lock lock_;
};

LayaModel::Session::Session(LayaModel* model,
                            std::unique_ptr<LayaQueryAdapter> query_adapter)
    : model_(model), query_adapter_(std::move(query_adapter)) {}

LayaModel::Session::Session(Session&&) noexcept = default;
LayaModel::Session& LayaModel::Session::operator=(Session&&) noexcept = default;
LayaModel::Session::~Session() = default;

base::expected<std::vector<std::vector<float>>, std::string>
LayaModel::Session::Execute(std::string_view input) {
  if (!model_ || !query_adapter_) {
    return base::unexpected("LayaModel::Session is not initialized.");
  }
  return model_->impl_->Execute(*query_adapter_, input);
}

// static
base::expected<std::unique_ptr<LayaModel>, std::string> LayaModel::Create(
    base::File model_file,
    base::File cache_file,
    int num_threads) {
  auto impl = std::make_unique<Impl>();
  RETURN_IF_ERROR(impl->Initialize(std::move(model_file), std::move(cache_file),
                                   num_threads));
  return base::WrapUnique(new LayaModel(std::move(impl)));
}

LayaModel::LayaModel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

LayaModel::~LayaModel() = default;

base::expected<LayaModel::Session, std::string> LayaModel::CreateSession(
    mojom::DecisionQueryPtr query) {
  if (!query) {
    return base::unexpected("DecisionQuery must not be null.");
  }
  ASSIGN_OR_RETURN(std::unique_ptr<LayaQueryAdapter> adapter,
                   impl_->CreateQueryAdapter(*query));
  return Session(this, std::move(adapter));
}

std::vector<size_t> LayaModel::GetSignatureLengthsForTesting() const {
  return impl_->GetSignatureLengths();
}

float LayaModel::GetCalibrationTemperatureForTesting(
    LayaQueryAdapter::QuestionType question_type) const {
  return impl_->GetCalibrationTemperature(question_type);
}

}  // namespace on_device_model
