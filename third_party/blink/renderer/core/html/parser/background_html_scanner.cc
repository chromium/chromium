// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

using CompileOptions = v8::ScriptCompiler::CompileOptions;

// Eager compilation takes more time and uses more memory than lazy compilation,
// but the resulting code executes faster. These options let us trade off
// between the pros/cons of eager and lazy compilation.
enum class CompileStrategy {
  // All scripts are compiled lazily.
  kLazy,
  // The first script in the chunk being scanned is compiled lazily, while the
  // rest are compiled eagerly. The first script usually needs to be parsed and
  // run soon after the body chunk is received, so using lazy compilation for
  // that script allows it to run sooner since lazy compilation will complete
  // faster.
  kFirstScriptLazy,
  // All scripts are compiled eagerly.
  kEager,
};

CompileOptions GetCompileOptions(bool first_script_in_scan) {
  static const base::FeatureParam<CompileStrategy>::Option
      kCompileStrategyOptions[] = {
          {CompileStrategy::kLazy, "lazy"},
          {CompileStrategy::kFirstScriptLazy, "first-script-lazy"},
          {CompileStrategy::kEager, "eager"},
      };

  static const base::FeatureParam<CompileStrategy> kCompileStrategyParam{
      &features::kPrecompileInlineScripts, "compile-strategy",
      CompileStrategy::kLazy, &kCompileStrategyOptions};

  switch (kCompileStrategyParam.Get()) {
    case CompileStrategy::kLazy:
      return CompileOptions::kNoCompileOptions;
    case CompileStrategy::kFirstScriptLazy:
      return first_script_in_scan ? CompileOptions::kNoCompileOptions
                                  : CompileOptions::kEagerCompile;
    case CompileStrategy::kEager:
      return CompileOptions::kEagerCompile;
  }
}

scoped_refptr<base::SequencedTaskRunner> GetCompileTaskRunner() {
  static const base::FeatureParam<bool> kCompileInParallelParam{
      &features::kPrecompileInlineScripts, "compile-in-parallel", true};
  // Returning a null task runner will result in posting to the worker pool for
  // each task.
  if (kCompileInParallelParam.Get())
    return nullptr;
  return worker_pool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
}

}  // namespace

// static
WTF::SequenceBound<BackgroundHTMLScanner> BackgroundHTMLScanner::Create(
    const HTMLParserOptions& options,
    ScriptableDocumentParser* parser) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Create");
  // The background scanner lives on one sequence, while the script streamers
  // work on a second sequence. This allows us to continue scanning the HTML
  // while scripts are compiling.
  return WTF::SequenceBound<BackgroundHTMLScanner>(
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}),
      std::make_unique<HTMLTokenizer>(options),
      WrapCrossThreadWeakPersistent(parser), GetCompileTaskRunner());
}

BackgroundHTMLScanner::BackgroundHTMLScanner(
    std::unique_ptr<HTMLTokenizer> tokenizer,
    ScriptableDocumentParser* parser,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : tokenizer_(std::move(tokenizer)),
      parser_(parser),
      task_runner_(std::move(task_runner)) {}

BackgroundHTMLScanner::~BackgroundHTMLScanner() = default;

void BackgroundHTMLScanner::Scan(const String& source) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Scan");
  first_script_in_scan_ = true;
  source_.Append(source);
  while (tokenizer_->NextToken(source_, token_)) {
    if (token_.GetType() == HTMLToken::kStartTag) {
      tokenizer_->UpdateStateFor(
          AttemptStaticStringCreation(token_.GetName(), kLikely8Bit));
    }
    ScanToken(token_);
    token_.Clear();
  }
}

void BackgroundHTMLScanner::ScanToken(const HTMLToken& token) {
  switch (token.GetType()) {
    case HTMLToken::kCharacter: {
      if (in_script_) {
        if (token.IsAll8BitData())
          script_builder_.Append(token.Data().AsString8());
        else
          script_builder_.Append(token.Data().AsString());
      }
      return;
    }
    case HTMLToken::kStartTag: {
      if (Match(TagImplFor(token.Data()), html_names::kScriptTag)) {
        in_script_ = true;
        script_builder_.Clear();
      }
      return;
    }
    case HTMLToken::kEndTag: {
      if (Match(TagImplFor(token.Data()), html_names::kScriptTag)) {
        in_script_ = false;
        // The script was empty, do nothing.
        if (script_builder_.IsEmpty())
          return;

        String script_text = script_builder_.ReleaseString();
        script_builder_.Clear();

        auto streamer = base::MakeRefCounted<BackgroundInlineScriptStreamer>(
            script_text, GetCompileOptions(first_script_in_scan_));
        first_script_in_scan_ = false;
        auto parser_lock = parser_.Lock();
        if (!parser_lock || !streamer->CanStream())
          return;

        parser_lock->AddInlineScriptStreamer(script_text, streamer);
        if (task_runner_) {
          PostCrossThreadTask(
              *task_runner_, FROM_HERE,
              CrossThreadBindOnce(&BackgroundInlineScriptStreamer::Run,
                                  std::move(streamer)));
        } else {
          worker_pool::PostTask(
              FROM_HERE, {base::TaskPriority::USER_BLOCKING},
              CrossThreadBindOnce(&BackgroundInlineScriptStreamer::Run,
                                  std::move(streamer)));
        }
      }
      return;
    }
    default: {
      return;
    }
  }
}

}  // namespace blink
