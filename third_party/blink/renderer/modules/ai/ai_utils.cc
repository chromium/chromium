// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_utils.h"

#include <algorithm>
#include <iterator>

namespace blink {

namespace {

mojom::blink::AISummarizerType ToMojoSummarizerType(V8AISummarizerType type) {
  switch (type.AsEnum()) {
    case V8AISummarizerType::Enum::kTlDr:
      return mojom::blink::AISummarizerType::kTLDR;
    case V8AISummarizerType::Enum::kKeyPoints:
      return mojom::blink::AISummarizerType::kKeyPoints;
    case V8AISummarizerType::Enum::kTeaser:
      return mojom::blink::AISummarizerType::kTeaser;
    case V8AISummarizerType::Enum::kHeadline:
      return mojom::blink::AISummarizerType::kHeadline;
  }
}

mojom::blink::AISummarizerFormat ToMojoSummarizerFormat(
    V8AISummarizerFormat format) {
  switch (format.AsEnum()) {
    case V8AISummarizerFormat::Enum::kPlainText:
      return mojom::blink::AISummarizerFormat::kPlainText;
    case V8AISummarizerFormat::Enum::kMarkdown:
      return mojom::blink::AISummarizerFormat::kMarkDown;
  }
}

mojom::blink::AISummarizerLength ToMojoSummarizerLength(
    V8AISummarizerLength length) {
  switch (length.AsEnum()) {
    case V8AISummarizerLength::Enum::kShort:
      return mojom::blink::AISummarizerLength::kShort;
    case V8AISummarizerLength::Enum::kMedium:
      return mojom::blink::AISummarizerLength::kMedium;
    case V8AISummarizerLength::Enum::kLong:
      return mojom::blink::AISummarizerLength::kLong;
  }
}

mojom::blink::AIWriterTone ToMojoAIWriterTone(V8AIWriterTone tone) {
  switch (tone.AsEnum()) {
    case V8AIWriterTone::Enum::kFormal:
      return mojom::blink::AIWriterTone::kFormal;
    case V8AIWriterTone::Enum::kNeutral:
      return mojom::blink::AIWriterTone::kNeutral;
    case V8AIWriterTone::Enum::kCasual:
      return mojom::blink::AIWriterTone::kCasual;
  }
}

mojom::blink::AIWriterFormat ToMojoAIWriterFormat(V8AIWriterFormat format) {
  switch (format.AsEnum()) {
    case V8AIWriterFormat::Enum::kPlainText:
      return mojom::blink::AIWriterFormat::kPlainText;
    case V8AIWriterFormat::Enum::kMarkdown:
      return mojom::blink::AIWriterFormat::kMarkdown;
  }
}

mojom::blink::AIWriterLength ToMojoAIWriterLength(V8AIWriterLength length) {
  switch (length.AsEnum()) {
    case V8AIWriterLength::Enum::kShort:
      return mojom::blink::AIWriterLength::kShort;
    case V8AIWriterLength::Enum::kMedium:
      return mojom::blink::AIWriterLength::kMedium;
    case V8AIWriterLength::Enum::kLong:
      return mojom::blink::AIWriterLength::kLong;
  }
}

mojom::blink::AIRewriterTone ToMojoAIRewriterTone(V8AIRewriterTone tone) {
  switch (tone.AsEnum()) {
    case V8AIRewriterTone::Enum::kAsIs:
      return mojom::blink::AIRewriterTone::kAsIs;
    case V8AIRewriterTone::Enum::kMoreFormal:
      return mojom::blink::AIRewriterTone::kMoreFormal;
    case V8AIRewriterTone::Enum::kMoreCasual:
      return mojom::blink::AIRewriterTone::kMoreCasual;
  }
}

mojom::blink::AIRewriterFormat ToMojoAIRewriterFormat(
    V8AIRewriterFormat format) {
  switch (format.AsEnum()) {
    case V8AIRewriterFormat::Enum::kAsIs:
      return mojom::blink::AIRewriterFormat::kAsIs;
    case V8AIRewriterFormat::Enum::kPlainText:
      return mojom::blink::AIRewriterFormat::kPlainText;
    case V8AIRewriterFormat::Enum::kMarkdown:
      return mojom::blink::AIRewriterFormat::kMarkdown;
  }
}

mojom::blink::AIRewriterLength ToMojoAIRewriterLength(
    V8AIRewriterLength length) {
  switch (length.AsEnum()) {
    case V8AIRewriterLength::Enum::kAsIs:
      return mojom::blink::AIRewriterLength::kAsIs;
    case V8AIRewriterLength::Enum::kShorter:
      return mojom::blink::AIRewriterLength::kShorter;
    case V8AIRewriterLength::Enum::kLonger:
      return mojom::blink::AIRewriterLength::kLonger;
  }
}

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptionsImpl(
    const AISummarizerCreateCoreOptions* options,
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
    const AIWriterCreateCoreOptions* options,
    const String& shared_context) {
  return mojom::blink::AIWriterCreateOptions::New(
      shared_context, ToMojoAIWriterTone(options->tone()),
      ToMojoAIWriterFormat(options->format()),
      ToMojoAIWriterLength(options->length()),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})),
      ToMojoLanguageCodes(options->getExpectedContextLanguagesOr({})),
      mojom::blink::AILanguageCode::New(
          options->getOutputLanguageOr(g_empty_string)));
}

mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptionsImpl(
    const AIRewriterCreateCoreOptions* options,
    const String& shared_context) {
  return mojom::blink::AIRewriterCreateOptions::New(
      shared_context, ToMojoAIRewriterTone(options->tone()),
      ToMojoAIRewriterFormat(options->format()),
      ToMojoAIRewriterLength(options->length()),
      ToMojoLanguageCodes(options->getExpectedInputLanguagesOr({})),
      ToMojoLanguageCodes(options->getExpectedContextLanguagesOr({})),
      mojom::blink::AILanguageCode::New(
          options->getOutputLanguageOr(g_empty_string)));
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

Vector<String> ToStringLanguageCodes(
    const Vector<mojom::blink::AILanguageCodePtr>& language_codes) {
  Vector<String> result;
  result.reserve(language_codes.size());
  std::ranges::transform(
      language_codes, std::back_inserter(result),
      [](const mojom::blink::AILanguageCodePtr& language_code) {
        return language_code->code;
      });
  return result;
}

base::expected<mojom::blink::AILanguageModelSamplingParamsPtr,
               SamplingParamsOptionError>
ResolveSamplingParamsOption(const AILanguageModelCreateCoreOptions* options) {
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
    const AISummarizerCreateOptions* options) {
  return ToMojoSummarizerCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptions(
    const AISummarizerCreateCoreOptions* core_options) {
  return ToMojoSummarizerCreateOptionsImpl(core_options, g_empty_string);
}

mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const AIWriterCreateOptions* options) {
  return ToMojoWriterCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}
mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const AIWriterCreateCoreOptions* core_options) {
  return ToMojoWriterCreateOptionsImpl(core_options, g_empty_string);
}
mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const AIRewriterCreateOptions* options) {
  return ToMojoRewriterCreateOptionsImpl(
      options, options->getSharedContextOr(g_empty_string));
}
mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const AIRewriterCreateCoreOptions* core_options) {
  return ToMojoRewriterCreateOptionsImpl(core_options, g_empty_string);
}

}  // namespace blink
