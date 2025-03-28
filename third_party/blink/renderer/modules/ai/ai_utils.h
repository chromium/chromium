// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_

#include "base/types/expected.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rewriter_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_summarizer_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_writer_create_options.h"

namespace blink {

class LanguageModelCreateCoreOptions;

static constexpr uint64_t kNormalizedDownloadProgressMax = 0x10000;

// Converts string language codes to AILanguageCode mojo struct.
Vector<mojom::blink::AILanguageCodePtr> ToMojoLanguageCodes(
    const Vector<String>& language_codes);

enum class SamplingParamsOptionError {
  kOnlyOneOfTopKAndTemperatureIsProvided,
  kInvalidTopK,
  kInvalidTemperature,
};
// Performs check on the sampling params option and return the constructed
// AILanguageModelSamplingParamsPtr if the option is valid, or a DOMException
// otherwise.
MODULES_EXPORT base::expected<mojom::blink::AILanguageModelSamplingParamsPtr,
                              SamplingParamsOptionError>
ResolveSamplingParamsOption(const LanguageModelCreateCoreOptions* options);

mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptions(
    const SummarizerCreateOptions* options);
mojom::blink::AISummarizerCreateOptionsPtr ToMojoSummarizerCreateOptions(
    const SummarizerCreateCoreOptions* core_options);
mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const WriterCreateOptions* options);
mojom::blink::AIWriterCreateOptionsPtr ToMojoWriterCreateOptions(
    const WriterCreateCoreOptions* core_options);
mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const RewriterCreateOptions* options);
mojom::blink::AIRewriterCreateOptionsPtr ToMojoRewriterCreateOptions(
    const RewriterCreateCoreOptions* core_options);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
