// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ISSUE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ISSUE_STORAGE_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class AuditsIssue;
class CoreProbeSink;
class InspectorIssue;
class ExecutionContext;

namespace protocol {
namespace Audits {
class InspectorIssue;
}  // namespace Audits
}  // namespace protocol

class CORE_EXPORT InspectorIssueStorage {
 public:
  InspectorIssueStorage();

  void AddInspectorIssue(CoreProbeSink*, InspectorIssue*);
  void AddInspectorIssue(CoreProbeSink*, mojom::blink::InspectorIssueInfoPtr);
  void AddInspectorIssue(ExecutionContext*,
                         mojom::blink::InspectorIssueInfoPtr);
  void AddInspectorIssue(ExecutionContext*, AuditsIssue);
  void AddInspectorIssue(CoreProbeSink*, AuditsIssue);

  void Clear();
  size_t size() const;
  protocol::Audits::InspectorIssue* at(size_t index) const;

  virtual ~InspectorIssueStorage();

 private:
  void AddInspectorIssue(CoreProbeSink*,
                         std::unique_ptr<protocol::Audits::InspectorIssue>);
  Deque<std::unique_ptr<protocol::Audits::InspectorIssue>> issues_;

  DISALLOW_COPY_AND_ASSIGN(InspectorIssueStorage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ISSUE_STORAGE_H_
