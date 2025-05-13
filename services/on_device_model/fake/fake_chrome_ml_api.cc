// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/fake/fake_chrome_ml_api.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace fake_ml {
namespace {

constexpr std::string_view kEos = "<eos>";

ChromeMLConstraintFns g_constraint_fns;

std::string PieceToString(const ml::InputPiece& piece) {
  if (std::holds_alternative<std::string>(piece)) {
    return std::get<std::string>(piece);
  }
  if (std::holds_alternative<ml::Token>(piece)) {
    switch (std::get<ml::Token>(piece)) {
      case ml::Token::kSystem:
        return "System: ";
      case ml::Token::kModel:
        return "Model: ";
      case ml::Token::kUser:
        return "User: ";
      case ml::Token::kEnd:
        return " End.";
    }
  }
  if (std::holds_alternative<SkBitmap>(piece)) {
    const SkBitmap& bitmap = std::get<SkBitmap>(piece);
    return base::StringPrintf("[Bitmap of size %dx%d]", bitmap.width(),
                              bitmap.height());
  }
  NOTREACHED();
}

std::string ReadFile(PlatformFile api_file) {
  base::File file(static_cast<base::PlatformFile>(api_file));
  std::vector<uint8_t> contents;
  contents.resize(file.GetLength());
  if (!file.ReadAndCheck(0, contents)) {
    return std::string();
  }
  return std::string(contents.begin(), contents.end());
}

std::string GenerateConstraintString(ChromeMLConstraint constraint) {
  // Prefer tokens in this order to try to keep output as short as possible.
  const std::vector<char> preferred = {
      '"', '}', ']', '{', '[', ':', ',',
  };
  ChromeMLConstraintMask mask;
  auto is_valid = [&](int i) {
    return !isspace(i) &&
           // SAFETY: Follows a C-API, sample mask will have vocab_size bits,
           // test-only code.
           UNSAFE_BUFFERS(mask.sample_mask[i / 32] & (1 << (i % 32)));
  };
  std::string result;
  // Breakk the loop if result is getting too long.
  while (result.size() < 100) {
    CHECK(g_constraint_fns.ComputeMask(constraint, mask))
        << g_constraint_fns.GetError(constraint);
    if (mask.is_stop) {
      break;
    }

    std::optional<uint32_t> token;
    // Try to grab a preferred token.
    for (char t : preferred) {
      if (is_valid(t)) {
        token = t;
        break;
      }
    }
    // If no preferred tokens are available, grab first valid token.
    if (!token) {
      for (int i = 0; i < 256; ++i) {
        if (is_valid(i)) {
          token = i;
          break;
        }
      }
    }
    g_constraint_fns.CommitToken(constraint, *token);
    result += std::string(1, static_cast<char>(*token));
  }
  return result;
}

// Simple tokenize fn for a single byte tokenizer which just passes through the
// bytes.
size_t TokenizeBytes(const void* user_data,
                     const uint8_t* bytes,
                     size_t bytes_len,
                     uint32_t* output_tokens,
                     size_t output_tokens_len) {
  for (size_t i = 0; i < std::min(bytes_len, output_tokens_len); ++i) {
    // SAFETY: Follows a C-API, test-only code. Bounds are length checked by the
    // loop.
    UNSAFE_BUFFERS(output_tokens[i]) = UNSAFE_BUFFERS(bytes[i]);
  }
  return bytes_len;
}

}  // namespace

void InitDawnProcs(const DawnProcTable& procs) {}

void SetMetricsFns(const ChromeMLMetricsFns* fns) {}

void SetFatalErrorFn(ChromeMLFatalErrorFn error_fn) {}

void SetFatalErrorNonGpuFn(ChromeMLFatalErrorFn error_fn) {}

bool GetEstimatedPerformance(ChromeMLPerformanceInfo* performance_info) {
  return false;
}

bool QueryGPUAdapter(void (*adapter_callback_fn)(WGPUAdapter adapter,
                                                 void* userdata),
                     void* userdata) {
  return false;
}

bool GetCapabilities(PlatformFile file, ChromeMLCapabilities& capabilities) {
  std::string contents = ReadFile(file);
  capabilities.image_input = contents.find("image") != std::string::npos;
  capabilities.audio_input = contents.find("audio") != std::string::npos;
  return true;
}

struct FakeModelInstance {
  ml::ModelBackendType backend_type;
  ml::ModelPerformanceHint performance_hint;
  std::string model_data;
};

struct FakeSessionInstance {
  raw_ptr<FakeModelInstance> model_instance;
  std::string adaptation_data;
  std::optional<uint32_t> adaptation_file_id;
  std::vector<std::string> context;
  bool cloned;
  bool enable_image_input;
  bool enable_audio_input;
  uint32_t top_k;
  float temperature;
};

struct FakeTsModelInstance {
  std::string model_data;
};

struct FakeCancelInstance {
  bool cancelled = false;
};

ChromeMLModel SessionCreateModel(const ChromeMLModelDescriptor* descriptor,
                                 uintptr_t context,
                                 ChromeMLScheduleFn schedule) {
  return reinterpret_cast<ChromeMLModel>(new FakeModelInstance{
      .backend_type = descriptor->backend_type,
      .performance_hint = descriptor->performance_hint,
  });
}

void DestroyModel(ChromeMLModel model) {
  auto* instance = reinterpret_cast<FakeModelInstance*>(model);
  delete instance;
}

ChromeMLSafetyResult ClassifyTextSafety(ChromeMLModel model,
                                        const char* text,
                                        float* scores,
                                        size_t* num_scores) {
  return ChromeMLSafetyResult::kNoClassifier;
}

ChromeMLSession CreateSession(ChromeMLModel model,
                              const ChromeMLAdaptationDescriptor* descriptor) {
  auto* model_instance = reinterpret_cast<FakeModelInstance*>(model);
  auto* instance = new FakeSessionInstance{};
  instance->model_instance = model_instance;
  if (descriptor) {
    instance->enable_image_input = descriptor->enable_image_input;
    instance->enable_audio_input = descriptor->enable_audio_input;
    instance->top_k = descriptor->top_k;
    instance->temperature = descriptor->temperature;
    if (descriptor->model_data) {
      instance->adaptation_file_id = descriptor->model_data->file_id;
      if (model_instance->backend_type == ml::ModelBackendType::kGpuBackend) {
        instance->adaptation_data =
            ReadFile(descriptor->model_data->weights_file);
      } else if (model_instance->backend_type ==
                 ml::ModelBackendType::kApuBackend) {
        base::ReadFileToString(
            base::FilePath::FromUTF8Unsafe(descriptor->model_data->model_path),
            &instance->adaptation_data);
      }
    }
  }
  return reinterpret_cast<ChromeMLSession>(instance);
}

ChromeMLSession CloneSession(ChromeMLSession session) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  return reinterpret_cast<ChromeMLSession>(new FakeSessionInstance{
      .model_instance = instance->model_instance,
      .adaptation_data = instance->adaptation_data,
      .adaptation_file_id = instance->adaptation_file_id,
      .context = instance->context,
      .cloned = true,
      .enable_image_input = instance->enable_image_input,
      .enable_audio_input = instance->enable_audio_input,
      .top_k = instance->top_k,
      .temperature = instance->temperature,
  });
}

void DestroySession(ChromeMLSession session) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  delete instance;
}

bool SessionAppend(ChromeMLSession session,
                   const ChromeMLAppendOptions* options,
                   ChromeMLCancel cancel) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  std::string text;
  for (size_t i = 0; i < options->input_size; i++) {
    // SAFETY: `options->input_size` describes how big `options->input` is.
    const ml::InputPiece& piece = UNSAFE_BUFFERS(options->input[i]);
    CHECK(!std::holds_alternative<SkBitmap>(piece) ||
          instance->enable_image_input);
    CHECK(!std::holds_alternative<ml::AudioBuffer>(piece) ||
          instance->enable_audio_input);
    text += PieceToString(piece);
  }
  if (options->max_tokens < text.size()) {
    text.resize(options->max_tokens);
  }

  if (!text.empty()) {
    instance->context.push_back(text);
  }
  if (options->context_saved_fn) {
    (*options->context_saved_fn)(static_cast<int>(text.size()));
  }
  return true;
}

bool SessionGenerate(ChromeMLSession session,
                     const ChromeMLGenerateOptions* options,
                     ChromeMLCancel cancel) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  auto OutputChunk = [output_fn =
                          *options->output_fn](const std::string& chunk) {
    ChromeMLExecutionOutput output = {};
    if (chunk.empty()) {
      output.status = ChromeMLExecutionStatus::kComplete;
      output_fn(&output);
      return;
    }
    output.status = ChromeMLExecutionStatus::kInProgress;
    output.text = chunk.c_str();
    output_fn(&output);
  };

  if (instance->model_instance->performance_hint ==
      ml::ModelPerformanceHint::kFastestInference) {
    OutputChunk("Fastest inference");
  }
  if (!instance->adaptation_data.empty()) {
    std::string adaptation_str = "Adaptation: " + instance->adaptation_data;
    if (instance->adaptation_file_id) {
      adaptation_str +=
          " (" + base::NumberToString(*instance->adaptation_file_id) + ")";
    }
    OutputChunk(adaptation_str);
  }

  // Only include sampling params if they're not the respective default values.
  if (instance->top_k != 1 || instance->temperature != 0) {
    OutputChunk(base::StrCat(
        {"TopK: ", base::NumberToString(instance->top_k),
         ", Temp: ", base::NumberToString(instance->temperature)}));
  }

  if (!instance->context.empty()) {
    for (const std::string& context : instance->context) {
      OutputChunk(context);
    }
  }
  if (options->constraint) {
    OutputChunk(GenerateConstraintString(options->constraint));
    g_constraint_fns.Delete(options->constraint);
  }
  OutputChunk("");
  return true;
}

bool SessionExecuteModel(ChromeMLSession session,
                         ChromeMLModel model,
                         const ChromeMLExecuteOptions* options,
                         ChromeMLCancel cancel) {
  ChromeMLAppendOptions append_opts{
      .input = options->input,
      .input_size = options->input_size,
      .max_tokens = options->max_tokens,
      .context_saved_fn = options->context_saved_fn,
  };
  if (!SessionAppend(session, &append_opts, cancel)) {
    return false;
  }
  if (!options->execution_output_fn) {
    return true;
  }
  ChromeMLGenerateOptions gen_opts{
      .max_output_tokens = options->max_output_tokens,
      .constraint = options->constraint,
      .output_fn = options->execution_output_fn,
  };
  return SessionGenerate(session, &gen_opts, cancel);
}

void SessionSizeInTokensInputPiece(ChromeMLSession session,
                                   ChromeMLModel model,
                                   const ml::InputPiece* input,
                                   size_t input_size,
                                   const ChromeMLSizeInTokensFn& fn) {
  std::string text;
  for (size_t i = 0; i < input_size; i++) {
    // SAFETY: `input_size` describes how big `input` is.
    const ml::InputPiece& piece = UNSAFE_BUFFERS(input[i]);
    if (!std::holds_alternative<std::string>(piece) &&
        !std::holds_alternative<ml::Token>(piece)) {
      continue;
    }

    text += PieceToString(piece);
  }
  fn(text.size());
}

void SessionScore(ChromeMLSession session,
                  const std::string& text,
                  const ChromeMLScoreFn& fn) {
  fn(static_cast<float>(text[0]));
}

ChromeMLCancel CreateCancel() {
  return reinterpret_cast<ChromeMLCancel>(new FakeCancelInstance());
}

void DestroyCancel(ChromeMLCancel cancel) {
  delete reinterpret_cast<FakeCancelInstance*>(cancel);
}

void CancelExecuteModel(ChromeMLCancel cancel) {
  auto* instance = reinterpret_cast<FakeCancelInstance*>(cancel);
  instance->cancelled = true;
}

void SetConstraintFns(const ChromeMLConstraintFns* fns) {
  g_constraint_fns = *fns;
}

bool GetTokenizerParams(ChromeMLModel model,
                        const ChromeMLGetTokenizerParamsFn& fn) {
  // Create a simple tokenizer mapping each byte to itself.
  std::string tokens;
  std::vector<uint32_t> token_lens;
  for (int i = 0; i < 256; ++i) {
    tokens += std::string(1, static_cast<char>(i));
    token_lens.push_back(1);
  }
  tokens += kEos;
  token_lens.push_back(kEos.size());
  ChromeMLTokenizerParams params{
      .vocab_size = static_cast<uint32_t>(token_lens.size()),
      .eos_token_id = static_cast<uint32_t>(token_lens.size() - 1),
      .token_lens = token_lens.data(),
      .token_bytes = reinterpret_cast<const uint8_t*>(tokens.data()),
      .tokenize_fn = &TokenizeBytes,
  };
  fn(params);
  return true;
}

ChromeMLTSModel CreateTSModel(const ChromeMLTSModelDescriptor* descriptor) {
  auto* instance = new FakeTsModelInstance{};
  return reinterpret_cast<ChromeMLTSModel>(instance);
}

void DestroyTSModel(ChromeMLTSModel model) {
  auto* instance = reinterpret_cast<FakeTsModelInstance*>(model);
  delete instance;
}

ChromeMLSafetyResult TSModelClassifyTextSafety(ChromeMLTSModel model,
                                               const char* text,
                                               float* scores,
                                               size_t* num_scores) {
  if (*num_scores != 2) {
    *num_scores = 2;
    return ChromeMLSafetyResult::kInsufficientStorage;
  }
  bool has_unsafe = std::string(text).find("unsafe") != std::string::npos;
  // SAFETY: Follows a C-API, num_scores checked above, test-only code.
  UNSAFE_BUFFERS(scores[0]) = has_unsafe ? 0.8 : 0.2;
  bool has_reasonable =
      std::string(text).find("reasonable") != std::string::npos;
  // SAFETY: Follows a C-API, num_scores checked above, test-only code.
  UNSAFE_BUFFERS(scores[1]) = has_reasonable ? 0.2 : 0.8;
  return ChromeMLSafetyResult::kOk;
}

const ChromeMLAPI g_api = {
    .InitDawnProcs = &InitDawnProcs,
    .SetMetricsFns = &SetMetricsFns,
    .SetFatalErrorFn = &SetFatalErrorFn,
    .ClassifyTextSafety = &ClassifyTextSafety,
    .DestroyModel = &DestroyModel,
    .GetEstimatedPerformance = &GetEstimatedPerformance,
    .QueryGPUAdapter = &QueryGPUAdapter,
    .GetCapabilities = &GetCapabilities,
    .SetFatalErrorNonGpuFn = &SetFatalErrorNonGpuFn,

    .SessionCreateModel = &SessionCreateModel,
    .SessionAppend = &SessionAppend,
    .SessionGenerate = &SessionGenerate,
    .SessionExecuteModel = &SessionExecuteModel,
    .SessionSizeInTokensInputPiece = &SessionSizeInTokensInputPiece,
    .SessionScore = &SessionScore,
    .CreateSession = &CreateSession,
    .CloneSession = &CloneSession,
    .DestroySession = &DestroySession,
    .CreateCancel = &CreateCancel,
    .DestroyCancel = &DestroyCancel,
    .CancelExecuteModel = &CancelExecuteModel,
    .SetConstraintFns = &SetConstraintFns,
    .GetTokenizerParams = &GetTokenizerParams,
    .ts_api =
        {
            .CreateModel = &CreateTSModel,
            .DestroyModel = &DestroyTSModel,
            .ClassifyTextSafety = &TSModelClassifyTextSafety,
        },
};

const ChromeMLAPI* GetFakeMlApi() {
  g_api.SetConstraintFns(ml::GetConstraintFns());
  return &g_api;
}

}  // namespace fake_ml
