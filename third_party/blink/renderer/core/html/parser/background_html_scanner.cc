// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

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

namespace WTF {

template <>
struct CrossThreadCopier<
    blink::BackgroundHTMLScanner::InlineScriptStreamerVector>
    : public CrossThreadCopierPassThrough<
          blink::BackgroundHTMLScanner::InlineScriptStreamerVector> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {
namespace {

// The maximum number of inline scripts that will be streamed per page.
int GetMaxScriptsPerPage() {
  static const base::FeatureParam<int> kMaxScriptsPerPage{
      &features::kPrecompileInlineScripts, "max-scripts-per-page", 50};
  return kMaxScriptsPerPage.Get();
}

}  // namespace

// static
WTF::SequenceBound<BackgroundHTMLScanner> BackgroundHTMLScanner::Create(
    const HTMLParserOptions& options,
    ScriptableDocumentParser* parser) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Create");

  // InlineScriptStreamer must be created on the main thread. We create as many
  // streamers as a page is likely to need here so the background scanner
  // doesn't have to go back to the main thread to create more.
  // TODO(crbug.com/v8/12916): Create these on the background thread.
  InlineScriptStreamerVector streamers;
  int scripts_per_page = GetMaxScriptsPerPage();
  streamers.ReserveInitialCapacity(scripts_per_page);
  for (int i = 0; i < scripts_per_page; i++)
    streamers.emplace_back(MakeGarbageCollected<InlineScriptStreamer>());

  // The background scanner lives on one sequence, while the script streamers
  // work on a second sequence. This allows us to continue scanning the HTML
  // while scripts are compiling.
  return WTF::SequenceBound<BackgroundHTMLScanner>(
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}),
      std::make_unique<HTMLTokenizer>(options), std::move(streamers),
      WrapCrossThreadWeakPersistent(parser),
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));
}

BackgroundHTMLScanner::BackgroundHTMLScanner(
    std::unique_ptr<HTMLTokenizer> tokenizer,
    InlineScriptStreamerVector streamers,
    ScriptableDocumentParser* parser,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : tokenizer_(std::move(tokenizer)),
      streamers_(std::move(streamers)),
      parser_(parser),
      task_runner_(std::move(task_runner)) {}

BackgroundHTMLScanner::~BackgroundHTMLScanner() = default;

void BackgroundHTMLScanner::Scan(const String& source) {
  TRACE_EVENT0("blink", "BackgroundHTMLScanner::Scan");
  source_.Append(source);
  while (tokenizer_->NextToken(source_, token_) && !streamers_.IsEmpty()) {
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
      if (in_script_)
        script_builder_.Append(token.Data().AsString());
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

        auto streamer = std::move(streamers_.back());
        streamers_.pop_back();
        auto parser_lock = parser_.Lock();
        if (!parser_lock || !streamer->CanStream())
          return;

        parser_lock->AddInlineScriptStreamer(script_text, streamer.Get());
        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBindOnce(&InlineScriptStreamer::Run, std::move(streamer),
                                script_text));
      }
      return;
    }
    default: {
      return;
    }
  }
}

}  // namespace blink
