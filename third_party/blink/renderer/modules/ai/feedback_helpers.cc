// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/feedback_helpers.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// FeedbackHelper is used to log feedback messages to the console only
// once per execution context for supported Built-In AI APIs.
class FeedbackHelper : public GarbageCollected<FeedbackHelper>,
                       public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  // Gets or creates the logger for the execution context.
  static FeedbackHelper& From(ExecutionContext& context) {
    FeedbackHelper* logger =
        Supplement<ExecutionContext>::From<FeedbackHelper>(context);
    if (!logger) {
      logger = MakeGarbageCollected<FeedbackHelper>(context);
      ProvideTo(context, logger);
    }
    return *logger;
  }

  explicit FeedbackHelper(ExecutionContext& context)
      : Supplement<ExecutionContext>(context) {}
  virtual ~FeedbackHelper() = default;

  bool HasLogged(AIMetrics::AISessionType session_type) const {
    return logged_session_types_.Contains(session_type);
  }

  void SetLogged(AIMetrics::AISessionType session_type) {
    logged_session_types_.insert(session_type);
  }

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  HashSet<AIMetrics::AISessionType> logged_session_types_;
};

const char FeedbackHelper::kSupplementName[] = "FeedbackHelper";

}  // namespace

void MaybeRequestFeedback(ScriptState* script_state,
                          AIMetrics::AISessionType session_type) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return;
  }
  FeedbackHelper& logger = FeedbackHelper::From(*context);

  if (logger.HasLogged(session_type)) {
    return;
  }

  StringView api_name;
  StringView component_id;

  switch (session_type) {
    case AIMetrics::AISessionType::kLanguageModel:
      api_name = "LanguageModel";
      component_id = "1583624";
      break;
    case AIMetrics::AISessionType::kWriter:
      api_name = "Writer";
      component_id = "1617227";
      break;
    case AIMetrics::AISessionType::kRewriter:
      api_name = "Rewriter";
      component_id = "1617227";
      break;
    case AIMetrics::AISessionType::kSummarizer:
      api_name = "Summarizer";
      component_id = "1617227";
      break;
    case AIMetrics::AISessionType::kProofreader:
      api_name = "Proofreader";
      component_id = "1827469";
      break;
    case AIMetrics::AISessionType::kLanguageDetector:
      api_name = "LanguageDetector";
      component_id = "1583316";
      break;
    case AIMetrics::AISessionType::kTranslator:
      api_name = "Translator";
      component_id = "1583316";
      break;
    default:
      component_id = "1583300";
  }

  logger.SetLogged(session_type);

  String message = blink::StrCat(
      {"This page uses Chrome's Built-In AI features (", api_name,
       ")! We're always improving our models; please submit your feedback "
       "here: https://issues.chromium.org/issues/new?component=",
       component_id});
  context->AddConsoleMessage(mojom::blink::ConsoleMessageSource::kJavaScript,
                             mojom::blink::ConsoleMessageLevel::kInfo, message);
}

}  // namespace blink
