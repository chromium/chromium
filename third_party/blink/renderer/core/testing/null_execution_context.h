// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NULL_EXECUTION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NULL_EXECUTION_CONTEXT_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class NullExecutionContext : public GarbageCollected<NullExecutionContext>,
                             public ExecutionContext {
 public:
  NullExecutionContext();
  ~NullExecutionContext() override;

  void SetURL(const KURL& url) { url_ = url; }

  const KURL& Url() const override { return url_; }
  const KURL& BaseURL() const override { return url_; }
  KURL CompleteURL(const String&) const override { return url_; }

  void DisableEval(const String&) override {}
  String UserAgent() const override { return String(); }

  HttpsState GetHttpsState() const override {
    return CalculateHttpsState(GetSecurityOrigin());
  }

  EventTarget* ErrorEventTarget() override { return nullptr; }

  void AddConsoleMessageImpl(ConsoleMessage*,
                             bool discard_duplicates) override {}
  void AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr) override {}
  void AddInspectorIssue(AuditsIssue) override {}
  void ExceptionThrown(ErrorEvent*) override {}

  void SetUpSecurityContextForTesting();

  ResourceFetcher* Fetcher() override { return nullptr; }
  bool CrossOriginIsolatedCapability() const override { return false; }
  bool DirectSocketCapability() const override { return false; }
  FrameOrWorkerScheduler* GetScheduler() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  void CountUse(mojom::WebFeature) override {}
  void CountDeprecation(mojom::WebFeature) override {}

  const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() const override;

  ExecutionContextToken GetExecutionContextToken() const final {
    return token_;
  }

 private:
  KURL url_;

  // A dummy scheduler to ensure that the callers of
  // ExecutionContext::GetScheduler don't have to check for whether it's null or
  // not.
  std::unique_ptr<FrameOrWorkerScheduler> scheduler_;

  // A fake token identifying this execution context.
  const LocalFrameToken token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NULL_EXECUTION_CONTEXT_H_
