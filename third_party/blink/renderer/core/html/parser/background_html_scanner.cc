// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
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
  // Cache the value to avoid parsing the param string more than once.
  static const bool kCompileInParallelValue = kCompileInParallelParam.Get();
  // Returning a null task runner will result in posting to the worker pool for
  // each task.
  if (kCompileInParallelValue)
    return nullptr;
  return worker_pool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
}

scoped_refptr<base::SequencedTaskRunner> GetTokenizeTaskRunner() {
  static const base::FeatureParam<bool> kTokenizeInParallelParam{
      &features::kPretokenizeCSS, "tokenize-in-parallel", true};
  // Cache the value to avoid parsing the param string more than once.
  static const bool kTokenizeInParallelValue = kTokenizeInParallelParam.Get();
  // Returning a null task runner will result in posting to the worker pool for
  // each task.
  if (kTokenizeInParallelValue)
    return nullptr;
  return worker_pool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
}

wtf_size_t GetMinimumScriptSize() {
  static const base::FeatureParam<int> kMinimumScriptSizeParam{
      &features::kPrecompileInlineScripts, "minimum-script-size", 0};
  // Cache the value to avoid parsing the param string more than once.
  static const wtf_size_t kMinimumScriptSizeValue =
      static_cast<wtf_size_t>(kMinimumScriptSizeParam.Get());
  return kMinimumScriptSizeValue;
}

wtf_size_t GetMinimumCSSSize() {
  static const base::FeatureParam<int> kMinimumCSSSizeParam{
      &features::kPretokenizeCSS, "minimum-css-size", 0};
  // Cache the value to avoid parsing the param string more than once.
  static const wtf_size_t kMinimumCSSSizeValue =
      static_cast<wtf_size_t>(kMinimumCSSSizeParam.Get());
  return kMinimumCSSSizeValue;
}

bool ShouldPrecompileFrame(bool is_main_frame) {
  if (!base::FeatureList::IsEnabled(features::kPrecompileInlineScripts))
    return false;

  static const base::FeatureParam<bool> kPrecompileMainFrameOnlyParam{
      &features::kPrecompileInlineScripts, "precompile-main-frame-only", false};
  // Cache the value to avoid parsing the param string more than once.
  static const bool kPrecompileMainFrameOnlyValue =
      kPrecompileMainFrameOnlyParam.Get();
  return is_main_frame || !kPrecompileMainFrameOnlyValue;
}

bool ShouldPretokenizeFrame(bool is_main_frame) {
  if (!base::FeatureList::IsEnabled(features::kPretokenizeCSS) ||
      !features::kPretokenizeInlineSheets.Get()) {
    return false;
  }

  static const base::FeatureParam<bool> kPretokenizeMainFrameOnlyParam{
      &features::kPretokenizeCSS, "pretokenize-main-frame-only", false};
  // Cache the value to avoid parsing the param string more than once.
  static const bool kPretokenizeMainFrameOnlyValue =
      kPretokenizeMainFrameOnlyParam.Get();
  return is_main_frame || !kPretokenizeMainFrameOnlyValue;
}

void TokenizeInlineCSS(const String& style_text,
                       ScriptableDocumentParser* parser) {
  if (!parser)
    return;

  TRACE_EVENT0("blink", "TokenizeInlineCSS");
  parser->AddCSSTokenizer(style_text,
                          CSSTokenizer::CreateCachedTokenizer(style_text));
}

}  // namespace

// static
WTF::SequenceBound<BackgroundHTMLScanner> BackgroundHTMLScanner::Create(
    const HTMLParserOptions& options,
    ScriptableDocumentParser* parser) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Create");
  auto token_scanner = ScriptTokenScanner::Create(parser);
  if (!token_scanner)
    return WTF::SequenceBound<BackgroundHTMLScanner>();
  // The background scanner lives on one sequence, while the script streamers
  // work on a second sequence. This allows us to continue scanning the HTML
  // while scripts are compiling.
  return WTF::SequenceBound<BackgroundHTMLScanner>(
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}),
      std::make_unique<HTMLTokenizer>(options), std::move(token_scanner));
}

BackgroundHTMLScanner::BackgroundHTMLScanner(
    std::unique_ptr<HTMLTokenizer> tokenizer,
    std::unique_ptr<ScriptTokenScanner> token_scanner)
    : tokenizer_(std::move(tokenizer)),
      token_scanner_(std::move(token_scanner)) {
  DCHECK(token_scanner_);
}

BackgroundHTMLScanner::~BackgroundHTMLScanner() = default;

void BackgroundHTMLScanner::Scan(const String& source) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Scan");
  token_scanner_->set_first_script_in_scan(true);
  source_.Append(source);
  while (HTMLToken* token = tokenizer_->NextToken(source_)) {
    if (token->GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(*token);
    token_scanner_->ScanToken(*token);
    token->Clear();
  }
}

std::unique_ptr<BackgroundHTMLScanner::ScriptTokenScanner>
BackgroundHTMLScanner::ScriptTokenScanner::Create(
    ScriptableDocumentParser* parser) {
  bool is_main_frame =
      parser->GetDocument() && parser->GetDocument()->IsInOutermostMainFrame();
  bool precompile_scripts = ShouldPrecompileFrame(is_main_frame);
  bool pretokenize_css = ShouldPretokenizeFrame(is_main_frame);
  if (!precompile_scripts && !pretokenize_css)
    return nullptr;

  return std::make_unique<ScriptTokenScanner>(
      parser,
      OptimizationParams{.task_runner = GetCompileTaskRunner(),
                         .min_size = GetMinimumScriptSize(),
                         .enabled = precompile_scripts},
      OptimizationParams{.task_runner = GetTokenizeTaskRunner(),
                         .min_size = GetMinimumCSSSize(),
                         .enabled = pretokenize_css});
}

BackgroundHTMLScanner::ScriptTokenScanner::ScriptTokenScanner(
    ScriptableDocumentParser* parser,
    OptimizationParams precompile_scripts_params,
    OptimizationParams pretokenize_css_params)
    : parser_(parser),
      precompile_scripts_params_(std::move(precompile_scripts_params)),
      pretokenize_css_params_(std::move(pretokenize_css_params)) {
  DCHECK(precompile_scripts_params_.enabled || pretokenize_css_params_.enabled);
}

void BackgroundHTMLScanner::ScriptTokenScanner::ScanToken(
    const HTMLToken& token) {
  switch (token.GetType()) {
    case HTMLToken::kCharacter: {
      if (in_tag_ != InsideTag::kNone) {
        if (token.IsAll8BitData())
          builder_.Append(token.Data().AsString8());
        else
          builder_.Append(token.Data().AsString());
      }
      return;
    }
    case HTMLToken::kStartTag: {
      if (precompile_scripts_params_.enabled &&
          Match(TagImplFor(token.Data()), html_names::kScriptTag)) {
        DCHECK_EQ(in_tag_, InsideTag::kNone);
        in_tag_ = InsideTag::kScript;
      } else if (pretokenize_css_params_.enabled &&
                 Match(TagImplFor(token.Data()), html_names::kStyleTag)) {
        DCHECK_EQ(in_tag_, InsideTag::kNone);
        in_tag_ = InsideTag::kStyle;
      } else {
        in_tag_ = InsideTag::kNone;
      }
      builder_.Clear();
      return;
    }
    case HTMLToken::kEndTag: {
      if (precompile_scripts_params_.enabled &&
          Match(TagImplFor(token.Data()), html_names::kScriptTag) &&
          in_tag_ == InsideTag::kScript) {
        in_tag_ = InsideTag::kNone;
        // The script was empty, do nothing.
        if (builder_.empty())
          return;

        String script_text = builder_.ReleaseString();
        builder_.Clear();

        if (script_text.length() < precompile_scripts_params_.min_size)
          return;

        auto streamer = base::MakeRefCounted<BackgroundInlineScriptStreamer>(
            script_text, GetCompileOptions(first_script_in_scan_));
        first_script_in_scan_ = false;
        auto parser_lock = parser_.Lock();
        if (!parser_lock || !streamer->CanStream())
          return;

        parser_lock->AddInlineScriptStreamer(script_text, streamer);
        if (precompile_scripts_params_.task_runner) {
          PostCrossThreadTask(
              *precompile_scripts_params_.task_runner, FROM_HERE,
              CrossThreadBindOnce(&BackgroundInlineScriptStreamer::Run,
                                  std::move(streamer)));
        } else {
          worker_pool::PostTask(
              FROM_HERE, {base::TaskPriority::USER_BLOCKING},
              CrossThreadBindOnce(&BackgroundInlineScriptStreamer::Run,
                                  std::move(streamer)));
        }
      } else if (pretokenize_css_params_.enabled &&
                 Match(TagImplFor(token.Data()), html_names::kStyleTag) &&
                 in_tag_ == InsideTag::kStyle) {
        in_tag_ = InsideTag::kNone;
        // The style was empty, do nothing.
        if (builder_.empty())
          return;

        String style_text = builder_.ReleaseString();
        builder_.Clear();

        if (style_text.length() < pretokenize_css_params_.min_size)
          return;

        // We don't need to tokenize duplicate stylesheets, as these will
        // already be cached. The set stores just the hash of the string to
        // optimize memory usage, and it's fine to do extra work in the rare
        // case of a hash collision.
        if (!css_text_hashes_.insert(style_text.Impl()->GetHash()).is_new_entry)
          return;

        if (pretokenize_css_params_.task_runner) {
          PostCrossThreadTask(
              *pretokenize_css_params_.task_runner, FROM_HERE,
              CrossThreadBindOnce(&TokenizeInlineCSS, style_text, parser_));
        } else {
          worker_pool::PostTask(
              FROM_HERE, {base::TaskPriority::USER_BLOCKING},
              CrossThreadBindOnce(&TokenizeInlineCSS, style_text, parser_));
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
