// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/proofreader.h"

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode_string.h"
#include "third_party/re2/src/re2/re2.h"

namespace blink {

// Represents two vector spans with matching subsequences; e.g.
// ['a', 'b', 'c', 'd'] and ['b', 'c', 'z'] -> {1, 0, 2}
struct MatchingBlock {
  // The start index of the matching subsequence in the first list.
  uint32_t start_a = 0;
  // The start index of the matching subsequence in the second list.
  uint32_t start_b = 0;
  // The size of the matching block.
  uint32_t size = 0;
};

// Start and end indices for a pair of subsequences from two vector spans.
struct BlockPair {
  size_t start_a;
  size_t end_a;
  size_t start_b;
  size_t end_b;
};

// Tokenize `text` by splitting it into a vector of words/spaces/punctuation.
Vector<String> Tokenize(const String& text) {
  Vector<String> tokens;
  if (text.empty()) {
    return tokens;
  }

  // Obtain a word break iterator for the entire string.
  // This iterator will find boundaries between words, punctuation, and spaces.
  TextBreakIterator* it = WordBreakIterator(text);

  if (!it) {
    return tokens;
  }

  // The iterator returns the indices of the boundaries. Iterate through them to
  // create substrings, which we consider as tokens.
  int32_t start = it->first();
  for (int32_t end = it->next(); end != -1; end = it->next()) {
    tokens.push_back(text.Substring(start, end - start));
    start = end;
  }

  return tokens;
}

// Find the longest matching block in a and b.
// If there are multiple maximal matching blocks,
// return one that starts earliest in a, and of all those maximal matching
// blocks that start earliest in a, return the one that starts earliest in b.
//
// I.e:
// a = [" ", "a", "b"],
// b = ["a", "b", " ", "a", "b"]
// returns {0, 2, 3}
MatchingBlock LongestCommonSubsequence(base::span<const String> a,
                                       base::span<const String> b) {
  MatchingBlock result;
  const size_t a_size = a.size();
  const size_t b_size = b.size();
  for (size_t i = 0; i < a_size; ++i) {
    for (size_t j = 0; j < b_size; ++j) {
      uint32_t size = 0;
      while (i + size < a_size && j + size < b_size &&
             a[i + size] == b[j + size]) {
        ++size;
      }
      if (size > result.size) {
        result.start_a = i;
        result.start_b = j;
        result.size = size;
      }
    }
  }
  return result;
}

Vector<MatchingBlock> GetMatchingBlocks(Vector<String> seq_1,
                                        Vector<String> seq_2) {
  base::span seq_1_span = seq_1;
  base::span seq_2_span = seq_2;
  // A queue of block pairs to compare from the two sequences.
  Deque<BlockPair> blocks_to_compare_queue;
  blocks_to_compare_queue.push_back(
      BlockPair(0, seq_1.size(), 0, seq_2.size()));

  Vector<MatchingBlock> matching_blocks;

  while (!blocks_to_compare_queue.empty()) {
    BlockPair block_pair = blocks_to_compare_queue.front();
    blocks_to_compare_queue.pop_front();
    // Find the longest common subsequence in the two subspans.
    MatchingBlock matching_block = LongestCommonSubsequence(
        seq_1_span.subspan(block_pair.start_a,
                           block_pair.end_a - block_pair.start_a),
        seq_2_span.subspan(block_pair.start_b,
                           block_pair.end_b - block_pair.start_b));
    if (matching_block.size == 0) {
      continue;
    }
    // Calculate the location of the matching block in the two original
    // sequences.
    uint32_t matching_start_in_seq_1 =
        block_pair.start_a + matching_block.start_a;
    uint32_t matching_start_in_seq_2 =
        block_pair.start_b + matching_block.start_b;
    matching_blocks.push_back(MatchingBlock(
        matching_start_in_seq_1, matching_start_in_seq_2, matching_block.size));
    // Push the remaining of the blocks to the left of the longest matching
    // block found in this iteration for further process.
    if (block_pair.start_a < matching_start_in_seq_1 &&
        block_pair.start_b < matching_start_in_seq_2) {
      blocks_to_compare_queue.push_back(
          BlockPair(block_pair.start_a, matching_start_in_seq_1,
                    block_pair.start_b, matching_start_in_seq_2));
    }
    // Push the remaining of the blocks to the right of the longest matching
    // block found in this iteration for further process.
    if (matching_start_in_seq_1 + matching_block.size < block_pair.end_a &&
        matching_start_in_seq_2 + matching_block.size < block_pair.end_b) {
      blocks_to_compare_queue.push_back(BlockPair(
          matching_start_in_seq_1 + matching_block.size, block_pair.end_a,
          matching_start_in_seq_2 + matching_block.size, block_pair.end_b));
    }
  }
  // Sort all matching blocks increasingly based on the start index in the
  // first sequence.
  std::sort(
      matching_blocks.begin(), matching_blocks.end(),
      [](MatchingBlock a, MatchingBlock b) { return a.start_a < b.start_a; });

  return matching_blocks;
}

Vector<Correction> FindDifferences(Vector<String> a, Vector<String> b) {
  // Index of next token to process.
  uint32_t a_index = 0;
  uint32_t b_index = 0;
  // Index for error location in the original string that corresponds with
  // the tokenized sequence a.
  uint32_t error_start_index = 0;
  uint32_t error_end_index = 0;
  // Index for correction location in the corrected string that corresponds
  // with the tokenized sequence b.
  uint32_t correction_start_index = 0;
  uint32_t correction_end_index = 0;

  Vector<Correction> corrections;

  Vector<MatchingBlock> matching_blocks = GetMatchingBlocks(a, b);

  // Insert zero block at the end to cover the case when there's no matching
  // block between the two sequences.
  matching_blocks.push_back(MatchingBlock({a.size(), b.size(), 0}));

  for (const MatchingBlock matching_block : matching_blocks) {
    // When there's difference in the two tokenized sequence before the next
    // matching block, a correction should be made to change from "a" to "b".
    if (a_index < matching_block.start_a || b_index < matching_block.start_b) {
      // Calculate error_end_index in the original string by accumulating
      // all the tokens' sizes.
      for (uint32_t i = a_index; i < matching_block.start_a; ++i) {
        error_end_index += a[i].length();
      }

      StringBuilder correction_text;
      // Calculate correction_end_index in the new string by accumulating
      // all the tokens' sizes.
      for (uint32_t i = b_index; i < matching_block.start_b; ++i) {
        correction_end_index += b[i].length();
        // Concatenate tokens to find the correction text.
        correction_text.Append(b[i]);
      }

      Correction c = Correction({error_start_index, error_end_index,
                                 correction_start_index, correction_end_index,
                                 correction_text.ReleaseString()});
      corrections.push_back(c);
    }
    // Increment error indexes to the next potential location of difference
    // in the original string.
    error_start_index = error_end_index;
    for (uint32_t i = matching_block.start_a;
         i < matching_block.start_a + matching_block.size; ++i) {
      error_start_index += a[i].length();
    }
    error_end_index = error_start_index;

    // Increment correction indexes to the next potential location of difference
    // in the new string.
    correction_start_index = correction_end_index;
    for (uint32_t i = matching_block.start_b;
         i < matching_block.start_b + matching_block.size; ++i) {
      correction_start_index += b[i].length();
    }
    correction_end_index = correction_start_index;

    // Increment index for processed tokens to the next potential location of
    // difference in the tokenized sequences
    a_index = matching_block.start_a + matching_block.size;
    b_index = matching_block.start_b + matching_block.size;
  }

  return corrections;
}

Vector<Correction> GetCorrections(const String& input,
                                  const String& corrected_input) {
  // Tokenize to find differences on token-level.
  Vector<String> tokenized_input = Tokenize(input);
  Vector<String> tokenized_corrected_input = Tokenize(corrected_input);

  Vector<Correction> corrections =
      FindDifferences(tokenized_input, tokenized_corrected_input);
  return corrections;
}

HeapVector<Member<ProofreadCorrection>> ToProofreadCorrections(
    Vector<Correction> raw_corrections) {
  HeapVector<Member<ProofreadCorrection>> corrections;
  for (const Correction& c : raw_corrections) {
    auto* correction = MakeGarbageCollected<ProofreadCorrection>();
    correction->setStartIndex(c.error_start);
    correction->setEndIndex(c.error_end);
    correction->setCorrection(c.correction);
    corrections.push_back(correction);
  }
  return corrections;
}

V8CorrectionType GetV8CorrectionTypeFromString(const String& type) {
  if (type == "Spelling") {
    return V8CorrectionType(V8CorrectionType::Enum::kSpelling);
  }
  if (type == "Punctuation") {
    return V8CorrectionType(V8CorrectionType::Enum::kPunctuation);
  }
  if (type == "Capitalization") {
    return V8CorrectionType(V8CorrectionType::Enum::kCapitalization);
  }
  if (type == "Preposition") {
    return V8CorrectionType(V8CorrectionType::Enum::kPreposition);
  }
  if (type == "Missing words") {
    return V8CorrectionType(V8CorrectionType::Enum::kMissingWords);
  }
  return V8CorrectionType(V8CorrectionType::Enum::kGrammar);
}

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIProofreader,
    mojom::blink::AIManagerCreateProofreaderClient,
    ProofreaderCreateOptions,
    Proofreader>::
    RemoteCreate(
        mojo::PendingRemote<mojom::blink::AIManagerCreateProofreaderClient>
          client_remote) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateProofreader(
      std::move(client_remote), ToMojoProofreaderCreateOptions(options_));
}

template <>
void AIWritingAssistanceCreateClient<
    mojom::blink::AIProofreader,
    mojom::blink::AIManagerCreateProofreaderClient,
    ProofreaderCreateOptions,
    Proofreader>::RemoteCanCreate(CanCreateCallback callback) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CanCreateProofreader(
      ToMojoProofreaderCreateOptions(options_), std::move(callback));
}

Proofreader::Proofreader(
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<mojom::blink::AIProofreader> pending_remote,
    ProofreaderCreateOptions* options)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      remote_(GetExecutionContext()),
      options_(std::move(options)),
      destruction_abort_controller_(AbortController::Create(script_state)),
      create_abort_signal_(options_->getSignalOr(nullptr)),
      task_runner_(std::move(task_runner)) {
  remote_.Bind(std::move(pending_remote), task_runner_);

  if (create_abort_signal_) {
    CHECK(!create_abort_signal_->aborted());
    create_abort_handle_ = create_abort_signal_->AddAlgorithm(
        BindOnce(&Proofreader::OnCreateAbortSignalAborted,
                 WrapWeakPersistent(this), WrapWeakPersistent(script_state)));
  }
}

void Proofreader::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
  visitor->Trace(options_);
  visitor->Trace(destruction_abort_controller_);
  visitor->Trace(create_abort_signal_);
  visitor->Trace(create_abort_handle_);
}

ScriptPromise<V8Availability> Proofreader::availability(
    ScriptState* script_state,
    ProofreaderCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }
  CHECK(options);
  if (!ValidateAndCanonicalizeOptionLanguages(script_state->GetIsolate(),
                                              options)) {
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  auto callback = BindOnce(
      [](ScriptPromiseResolver<V8Availability>* resolver,
         ExecutionContext* execution_context,
         mojom::blink::ModelAvailabilityCheckResult result) {
        Availability availability = HandleModelAvailabilityCheckResult(
            execution_context, AIMetrics::AISessionType::kProofreader, result);
        resolver->Resolve(AvailabilityToV8(availability));
      },
      WrapPersistent(resolver), WrapPersistent(execution_context));
  ai_manager_remote->CanCreateProofreader(
      ToMojoProofreaderCreateOptions(options), std::move(callback));

  return promise;
}

ScriptPromise<Proofreader> Proofreader::create(
    ScriptState* script_state,
    ProofreaderCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<Proofreader>();
  }

  CHECK(options);
  if (!ValidateAndCanonicalizeOptionLanguages(script_state->GetIsolate(),
                                              options)) {
    return ScriptPromise<Proofreader>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Proofreader>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);

  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<AIWritingAssistanceCreateClient<
      mojom::blink::AIProofreader,
      mojom::blink::AIManagerCreateProofreaderClient, ProofreaderCreateOptions,
      Proofreader>>(script_state, resolver, options);
  return promise;
}

ScriptPromise<ProofreadResult> Proofreader::proofread(
    ScriptState* script_state,
    const String& input,
    const ProofreaderProofreadOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }

  CHECK(options);
  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  if (!remote_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<ProofreadResult>();
  }

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kProofreader),
                             static_cast<int>(input.CharactersSizeInBytes()));

  // Resolver and Promise for the final proofread() result.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<ProofreadResult>>(
      script_state);
  auto promise = resolver->Promise();

  String trimmed_input = input.StripWhiteSpace();
  if (trimmed_input.empty()) {
    auto* proofread_result = MakeGarbageCollected<ProofreadResult>();
    proofread_result->setCorrectedInput(input);
    resolver->Resolve(std::move(proofread_result));
    return promise;
  }

  // Step 1: Prompt the model to proofread and return fully corrected text.
  // Pass persistent refs to keep this instance alive during the response.
  auto pending_remote = CreateModelExecutionResponder(
      script_state, composite_signal, task_runner_,
      AIMetrics::AISessionType::kProofreader,

      BindOnce(&Proofreader::OnProofreadComplete, WrapPersistent(this),
               WrapPersistent(resolver), WrapPersistent(script_state),
               WrapPersistent(composite_signal), input),
      /*overflow_callback=*/base::DoNothingWithBoundArgs(WrapPersistent(this)),
      BindOnce(&Proofreader::OnProofreadError, WrapPersistent(this),
               WrapPersistent(resolver)),
      BindOnce(&Proofreader::OnProofreadAbort, WrapPersistent(this),
               WrapPersistent(resolver), WrapPersistent(composite_signal),
               WrapPersistent(script_state)));
  remote_->Proofread(input, std::move(pending_remote));

  return promise;
}

void Proofreader::destroy(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  remote_.reset();
  destruction_abort_controller_->abort(script_state);
  DestroyImpl();
}

// TODO(crbug.com424659255): Consolidate with AIWritingAssistanceBase.

void Proofreader::DestroyImpl() {
  remote_.reset();

  if (create_abort_handle_) {
    create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
    create_abort_handle_ = nullptr;
  }
}

void Proofreader::OnCreateAbortSignalAborted(ScriptState* script_state) {
  if (script_state) {
    destruction_abort_controller_->abort(
        script_state, create_abort_signal_->reason(script_state));
  }
  DestroyImpl();
}

AbortSignal* Proofreader::CreateCompositeSignal(
    ScriptState* script_state,
    const ProofreaderProofreadOptions* options) {
  HeapVector<Member<AbortSignal>> signals;

  signals.push_back(destruction_abort_controller_->signal());

  CHECK(options);
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  return MakeGarbageCollected<AbortSignal>(script_state, signals);
}

bool Proofreader::ValidateAndCanonicalizeOptionLanguages(
    v8::Isolate* isolate,
    ProofreaderCreateCoreOptions* options) {
  using LanguageList = std::optional<Vector<String>>;
  if (options->hasExpectedInputLanguages()) {
    LanguageList result = ValidateAndCanonicalizeBCP47Languages(
        isolate, options->expectedInputLanguages());
    if (!result) {
      return false;
    }
    options->setExpectedInputLanguages(*result);
  }

  if (options->hasCorrectionExplanationLanguage()) {
    LanguageList result = ValidateAndCanonicalizeBCP47Languages(
        isolate, {options->correctionExplanationLanguage()});
    if (!result) {
      return false;
    }
    options->setCorrectionExplanationLanguage((*result)[0]);
  }
  return true;
}

void Proofreader::OnProofreadComplete(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    ScriptState* script_state,
    AbortSignal* signal,
    const String& input,
    const String& corrected_input,
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  DCHECK(resolver);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return;
  }
  auto* proofread_result = MakeGarbageCollected<ProofreadResult>();
  proofread_result->setCorrectedInput(corrected_input);
  // Step 2: Find list of corrections by comparing original input and fully
  // corrected input from model execution
  auto raw_corrections = GetCorrections(input, corrected_input);
  auto corrections = ToProofreadCorrections(raw_corrections);
  proofread_result->setCorrections(corrections);

  // Resolve if correction types and explanations are not requested.
  if (!options_->includeCorrectionTypes() || corrections.empty()) {
    resolver->Resolve(proofread_result);
    return;
  }

  // Step 3: Fetch correction type labels for all corrections, if requested.
  // Labels are fetched one-by-one, and when all labels are received,
  // GetCorrectionTypes will be responsible for resolving the promise for
  // proofread() with the `proofread_result`.
  GetCorrectionTypes(resolver, script_state, signal, proofread_result,
                     raw_corrections, input, 0);
}

void Proofreader::OnProofreadError(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    DOMException* exception) {
  resolver->Reject(exception);
}

void Proofreader::OnProofreadAbort(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    AbortSignal* signal,
    ScriptState* script_state) {
  resolver->Reject(signal->reason(script_state));
}

void Proofreader::GetCorrectionTypes(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    ScriptState* script_state,
    AbortSignal* signal,
    ProofreadResult* result,
    Vector<Correction> raw_corrections,
    const String& input,
    uint32_t correction_index) {
  // Done getting all correction type labels.
  if (correction_index == result->corrections().size()) {
    resolver->Resolve(result);
    return;
  }

  // Get correction type label for the next correction.
  auto correction = raw_corrections[correction_index];

  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, task_runner_,
      AIMetrics::AISessionType::kProofreader,
      BindOnce(&Proofreader::OnLabelComplete, WrapPersistent(this),
               WrapPersistent(resolver), WrapPersistent(script_state),
               WrapPersistent(signal), WrapPersistent(result), raw_corrections,
               input, correction_index),
      /*overflow_callback=*/
      base::DoNothingWithBoundArgs(WrapPersistent(this)),
      /*error_callback=*/
      BindOnce([](ScriptPromiseResolver<ProofreadResult>* resolver,
                  DOMException* exception) { resolver->Reject(exception); },
               WrapPersistent(resolver)),
      /*abort_callback=*/
      BindOnce(
          [](ScriptPromiseResolver<ProofreadResult>* resolver,
             AbortSignal* signal, ScriptState* script_state) {
            resolver->Reject(signal->reason(script_state));
          },
          WrapPersistent(resolver), WrapPersistent(signal),
          WrapPersistent(script_state)));

  String from = input.Substring(correction.error_start,
                                correction.error_end - correction.error_start);
  String to = correction.correction;
  String correction_instruction =
      StrCat({"Correcting `", from, "` to `", to, "`"});

  // Annotate the current error in the original input.
  String input_with_error =
      StrCat({input.Substring(0, correction.error_start), "`", from, "`",
              input.Substring(correction.error_end)});

  // Annotate the current correction in the corrected input.
  String corrected_input = result->correctedInput();
  String corrected_input_with_correction =
      StrCat({corrected_input.Substring(0, correction.correction_start), "`",
              to, "`", corrected_input.Substring(correction.correction_end)});

  remote_->GetCorrectionType(input_with_error, corrected_input_with_correction,
                             correction_instruction, std::move(pending_remote));
}

void Proofreader::OnLabelComplete(
    ScriptPromiseResolver<ProofreadResult>* resolver,
    ScriptState* script_state,
    AbortSignal* signal,
    ProofreadResult* result,
    Vector<Correction> raw_corrections,
    const String& input,
    uint32_t correction_index,
    const String& model_response,
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  DCHECK(resolver);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return;
  }

  // Default correction type
  String label = "Grammar";

  // Parse the label from the response of the format {"label": "label0"}
  RE2 pattern("{\"label\":\\s*\"([^\"]+)\"}");
  StringUtf8Adaptor adaptor(model_response);
  std::string_view response = adaptor.AsStringView();
  std::string_view label_value;
  if (RE2::FullMatch(response, pattern, &label_value)) {
    label = String::FromUTF8(label_value);
  }
  result->corrections()[correction_index]->setType(
      GetV8CorrectionTypeFromString(label));

  uint32_t next_index = correction_index + 1;

  GetCorrectionTypes(resolver, script_state, signal, result, raw_corrections,
                     input, next_index);
}

}  // namespace blink
