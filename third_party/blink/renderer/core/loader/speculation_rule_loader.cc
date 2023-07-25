// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/header_util.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/resource/speculation_rules_resource.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"

namespace blink {

SpeculationRuleLoader::SpeculationRuleLoader(Document& document)
    : document_(document) {}

SpeculationRuleLoader::~SpeculationRuleLoader() = default;

void SpeculationRuleLoader::LoadResource(SpeculationRulesResource* resource,
                                         const KURL& base_url) {
  DCHECK(!resource_);
  base_url_ = base_url;
  resource_ = resource;
  resource_->AddFinishObserver(
      this, document_->GetTaskRunner(TaskType::kNetworking).get());
  start_time_ = base::TimeTicks::Now();
  DocumentSpeculationRules::From(*document_).AddSpeculationRuleLoader(this);
}

void SpeculationRuleLoader::NotifyFinished() {
  DCHECK(resource_);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.SpeculationRules.FetchTime",
                             base::TimeTicks::Now() - start_time_);

  int response_code = resource_->GetResponse().HttpStatusCode();
  if (!network::IsSuccessfulStatus(response_code)) {
    CountSpeculationRulesLoadOutcome(
        SpeculationRulesLoadOutcome::kInvalidStatusCode);
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Received a response with unsuccessful status code (" +
            String::Number(response_code) + ") for rule set requested from \"" +
            resource_->GetResourceRequest().Url().ElidedString() +
            "\" found in Speculation-Rules header."));
    return;
  }

  if (!EqualIgnoringASCIICase(resource_->HttpContentType(),
                              "application/speculationrules+json")) {
    CountSpeculationRulesLoadOutcome(
        SpeculationRulesLoadOutcome::kInvalidMimeType);
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Received a response with invalid MIME type \"" +
            resource_->HttpContentType() +
            "\" for the rule set requested from \"" +
            resource_->GetResourceRequest().Url().ElidedString() +
            "\" found in the Speculation-Rules header."));
    return;
  }
  if (!resource_->HasData()) {
    CountSpeculationRulesLoadOutcome(
        SpeculationRulesLoadOutcome::kEmptyResponseBody);
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Received a response with no data for rule set \"" +
            resource_->GetResourceRequest().Url().ElidedString() +
            "\" found in Speculation-Rules "
            "header."));
    return;
  }

  const auto& source_text = resource_->DecodedText();
  auto* source = MakeGarbageCollected<SpeculationRuleSet::Source>(
      source_text, base_url_, resource_->InspectorId());
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document_->GetExecutionContext());
  CHECK(rule_set);
  DocumentSpeculationRules::From(*document_).AddRuleSet(rule_set);
  rule_set->AddConsoleMessageForValidation(*document_, *resource_);
  resource_->RemoveFinishObserver(this);
  resource_ = nullptr;
  DocumentSpeculationRules::From(*document_).RemoveSpeculationRuleLoader(this);
}

void SpeculationRuleLoader::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(resource_);
  ResourceFinishObserver::Trace(visitor);
}

}  // namespace blink
