// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_utils.h"

#include <algorithm>
#include <iterator>

#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_core_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_features.h"

namespace blink {

namespace {

mojom::blink::AISummarizerType ToMojoSummarizerType(V8SummarizerType type) {
  switch (type.AsEnum()) {
    case V8SummarizerType::Enum::kTldr:
      return mojom::blink::AISummarizerType::kTLDR;
    case V8SummarizerType::Enum::kKeyPoints:
      return mojom::blink::AISummarizerType::kKeyPoints;
    case V8SummarizerType::Enum::kTeaser:
      return mojom::blink::AISummarizerType::kTeaser;
    case V8SummarizerType::Enum::kHeadline:
      return mojom::blink::AISummarizerType::kHeadline;
  }
}

mojom::blink::AISummarizerFormat ToMojoSummarizerFormat(
    V8SummarizerFormat format) {
  switch (format.AsEnum()) {
    case V8SummarizerFormat::Enum::kPlainText:
      return mojom::blink::AISummarizerFormat::kPlainText;
    case V8SummarizerFormat::Enum::kMarkdown:
      return mojom::blink::AISummarizerFormat::kMarkDown;
  }
}

mojom::blink::AISummarizerLength ToMojoSummarizerLength(
    V8SummarizerLength length) {
  switch (length.AsEnum()) {
    case V8SummarizerLength::Enum::kShort:
      return mojom::blink::AISummarizerLength::kShort;
    case V8SummarizerLength::Enum::kMedium:
      return mojom::blink::AISummarizerLength::kMedium;
    case V8SummarizerLength::Enum::kLong:
      return mojom::blink::AISummarizerLength::kLong;
  }
}

mojom::blink::AIWriterTone ToMojoWriterTone(V8WriterTone tone) {
  switch (tone.AsEnum()) {
    case V8WriterTone::Enum::kFormal:
      return mojom::blink::AIWriterTone::kFormal;
    case V8WriterTone::Enum::kNeutral:
      return mojom::blink::AIWriterTone::kNeutral;
    case V8WriterTone::Enum::kCasual:
      return mojom::blink::AIWriterTone::kCasual;
  }
}

mojom::blink::AIWriterFormat ToMojoWriterFormat(V8WriterFormat format) {
  switch (format.AsEnum()) {
    case V8WriterFormat::Enum::kPlainText:
      return mojom::blink::AIWriterFormat::kPlainText;
    case V8WriterFormat::Enum::kMarkdown:
      return mojom::blink::AIWriterFormat::kMarkdown;
  }
}

mojom::blink::AIWriterLength ToMojoWriterLength(V8WriterLength length) {
  switch (length.AsEnum()) {
    case V8WriterLength::Enum::kShort:
      return mojom::blink::AIWriterLength::kShort;
    case V8WriterLength::Enum::kMedium:
      return mojom::blink::AIWriterLength::kMedium;
    case V8WriterLength::Enum::kLong:
      return mojom::blink::AIWriterLength::kLong;
  }
}

mojom::blink::AIRewriterTone ToMojoRewriterTone(V8RewriterTone tone) {
  switch (tone.AsEnum()) {
    case V8RewriterTone::Enum::kAsIs:
      return mojom::blink::AIRewriterTone::kAsIs;
    case V8RewriterTone::Enum::kMoreFormal:
      return mojom::blink::AIRewriterTone::kMoreFormal;
    case V8RewriterTone::Enum::kMoreCasual:
      return mojom::blink::AIRewriterTone::kMoreCasual;
  }
}

mojom::blink::AIRewriterFormat ToMojoRewriterFormat(V8RewriterFormat format) {
  switch (format.AsEnum()) {
    case V8RewriterFormat::Enum::kAsIs:
      return mojom::blink::AIRewriterFormat::kAsIs;
    case V8RewriterFormat::Enum::kPlainText:
      return mojom::blink::AIRewriterFormat::kPlainText;
    case V8RewriterFormat::Enum::kMarkdown:
      return mojom::blink::AIRewriterFormat::kMarkdown;
  }
}

mojom::blink::AIRewriterLength ToMojoRewriterLength(V8RewriterLength length) {
  switch (length.AsEnum()) {
    case V8RewriterLength::Enum::kAsIs:
      return mojom::blink::AIRewriterLength::kAsIs;
    case V8RewriterLength::Enum::kShorter:
      return mojom::blink::AIRewriterLength::kShorter;
    case V8RewriterLength::Enum::kLonger:
      return mojom::blink::AIRewriterLength::kLonger;
  }
}

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptionsImpl(
    const SummarizerCreateCoreOptions* options,
    const String& shared_context) {
  return mojom::blink::AISummarizerCreateOptions::New(
      shared_context, ToMojoSummarizerType(options->type()),
      ToMojoSummarizerFormat(options->format()),
      ToMojoSummarizerLength(options->length()),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})),
      ToMojoLanguageCodes(options->getExpectedContextLanguagesOr({})),
      mojom::blink::AILanguageCode::New(
          options->getOutputLanguageOr(g_empty_string)));
}

mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptionsImpl(
    const WriterCreateCoreOptions* options,
    const String& shared_context) {
  return mojom::blink::AIWriterCreateOptions::New(
      shared_context, ToMojoWriterTone(options->tone()),
      ToMojoWriterFormat(options->format()),
      ToMojoWriterLength(options->length()),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})),
      ToMojoLanguageCodes(options->getExpectedContextLanguagesOr({})),
      mojom::blink::AILanguageCode::New(
          options->getOutputLanguageOr(g_empty_string)));
}

mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptionsImpl(
    const RewriterCreateCoreOptions* options,
    const String& shared_context) {
  return mojom::blink::AIRewriterCreateOptions::New(
      shared_context, ToMojoRewriterTone(options->tone()),
      ToMojoRewriterFormat(options->format()),
      ToMojoRewriterLength(options->length()),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})),
      ToMojoLanguageCodes(options->getExpectedContextLanguagesOr({})),
      mojom::blink::AILanguageCode::New(
          options->getOutputLanguageOr(g_empty_string)));
}

mojom::blink::AIProofreaderCreateOptionsPtr ToMojoProofreaderCreateOptionsImpl(
    const ProofreaderCreateCoreOptions* options) {
  return mojom::blink::AIProofreaderCreateOptions::New(
      options->includeCorrectionTypes(),
      options->includeCorrectionExplanations(),
      mojom::blink::AILanguageCode::New(
          options->getCorrectionExplanationLanguageOr(g_empty_string)),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})));
}

mojom::blink::AILanguageModelPromptType ToMojoInputType(
    V8LanguageModelMessageType type) {
  switch (type.AsEnum()) {
    case V8LanguageModelMessageType::Enum::kText:
      return mojom::blink::AILanguageModelPromptType::kText;
    case V8LanguageModelMessageType::Enum::kAudio:
      return mojom::blink::AILanguageModelPromptType::kAudio;
    case V8LanguageModelMessageType::Enum::kImage:
      return mojom::blink::AILanguageModelPromptType::kImage;
    case V8LanguageModelMessageType::Enum::kToolCall:
      return mojom::blink::AILanguageModelPromptType::kToolCall;
    case V8LanguageModelMessageType::Enum::kToolResponse:
      return mojom::blink::AILanguageModelPromptType::kToolResponse;
  }
}

}  // namespace

Vector<mojom::blink::AILanguageCodePtr> ToMojoLanguageCodes(
    const Vector<String>& language_codes) {
  Vector<mojom::blink::AILanguageCodePtr> result;
  result.reserve(language_codes.size());
  std::ranges::transform(
      language_codes, std::back_inserter(result),
      [](const String& language_code) {
        return mojom::blink::AILanguageCode::New(language_code);
      });
  return result;
}

Vector<mojom::blink::AILanguageModelExpectedPtr> ToMojoExpectations(
    const HeapVector<Member<LanguageModelExpected>>& expected_inputs) {
  Vector<mojom::blink::AILanguageModelExpectedPtr> result;
  result.reserve(expected_inputs.size());
  std::ranges::transform(
      expected_inputs, std::back_inserter(result),
      [](const Member<LanguageModelExpected>& expected_input) {
        auto value = mojom::blink::AILanguageModelExpected::New();
        value->type = ToMojoInputType(expected_input->type());
        if (expected_input->hasLanguages()) {
          value->languages = ToMojoLanguageCodes(expected_input->languages());
        }
        return value;
      });
  return result;
}

base::expected<mojom::blink::AILanguageModelSamplingParamsPtr,
               SamplingParamsOptionError>
ResolveSamplingParamsOption(const LanguageModelCreateCoreOptions* options) {
  if (!options || (!options->hasTopK() && !options->hasTemperature())) {
    return nullptr;
  }

  // The temperature and top_k are optional, but they must be provided
  // together.
  if (options->hasTopK() != options->hasTemperature()) {
    return base::unexpected(
        SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided);
  }

  // The `topK` value must be greater than 1.
  if (options->topK() < 1) {
    return base::unexpected(SamplingParamsOptionError::kInvalidTopK);
  }

  // The `temperature` value must be greater than 0.
  if (options->temperature() < 0) {
    return base::unexpected(SamplingParamsOptionError::kInvalidTemperature);
  }

  return mojom::blink::AILanguageModelSamplingParams::New(
      options->topK(), options->temperature());
}

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptions(
    const SummarizerCreateOptions* options) {
  return ToMojoSummarizerCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptions(
    const SummarizerCreateCoreOptions* core_options) {
  return ToMojoSummarizerCreateOptionsImpl(core_options, g_empty_string);
}

mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const WriterCreateOptions* options) {
  return ToMojoWriterCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}

mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const WriterCreateCoreOptions* core_options) {
  return ToMojoWriterCreateOptionsImpl(core_options, g_empty_string);
}

mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const RewriterCreateOptions* options) {
  return ToMojoRewriterCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}

mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const RewriterCreateCoreOptions* core_options) {
  return ToMojoRewriterCreateOptionsImpl(core_options, g_empty_string);
}

mojom::blink::AIProofreaderCreateOptionsPtr ToMojoProofreaderCreateOptions(
    const ProofreaderCreateOptions* options) {
  return ToMojoProofreaderCreateOptionsImpl(options);
}

mojom::blink::AIProofreaderCreateOptionsPtr ToMojoProofreaderCreateOptions(
    const ProofreaderCreateCoreOptions* core_options) {
  return ToMojoProofreaderCreateOptionsImpl(core_options);
}

std::optional<Vector<String>> ValidateAndCanonicalizeBCP47Languages(
    v8::Isolate* isolate,
    const Vector<String>& languages) {
  Vector<String> canonicalized_languages;
  for (const String& language : languages) {
    // Throws RangeError if `language` is not a valid language tag.
    v8::Maybe<std::string> maybe_canonical_language =
        isolate->ValidateAndCanonicalizeUnicodeLocaleId(language.Ascii());
    if (maybe_canonical_language.IsNothing()) {
      return std::nullopt;
    }

    String canonical_language(maybe_canonical_language.FromJust());

    if (!canonicalized_languages.Contains(canonical_language)) {
      canonicalized_languages.emplace_back(std::move(canonical_language));
    }
  }
  return canonicalized_languages;
}

bool RequiresUserActivation(Availability availability) {
  return availability == Availability::kDownloadable;
}

bool MeetsUserActivationRequirements(LocalDOMWindow* window) {
  LocalFrame* frame = window->GetFrame();
  if (!frame) {
    return false;
  }

  if (base::FeatureList::IsEnabled(kAIRelaxUserActivationReqs)) {
    return frame->HasStickyUserActivation();
  } else {
    return LocalFrame::ConsumeTransientUserActivation(frame);
  }
}

RunOnDestruction::RunOnDestruction(base::OnceClosure callback)
    : callback_(std::move(callback)) {}

RunOnDestruction::~RunOnDestruction() {
  if (!callback_.is_null()) {
    std::move(callback_).Run();
  }
}

void RunOnDestruction::Reset() {
  callback_.Reset();
}

}  // namespace blink
