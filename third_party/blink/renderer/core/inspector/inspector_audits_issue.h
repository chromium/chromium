// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

namespace protocol {
namespace Audits {
class InspectorIssue;
}
}  // namespace protocol

// |AuditsIssue| is a thin wrapper around the Audits::InspectorIssue
// protocol class.
//
// There are a few motiviations for this class:
//  1) Prevent leakage of auto-generated CDP resources into the
//     rest of blink.
//  2) Control who can assemble Audits::InspectorIssue's as this should
//     happen |inspector| land.
//  3) Prevent re-compilation of various blink classes when the protocol
//     changes. The protocol type can be forward declared in header files,
//     but for the std::unique_ptr, the generated |Audits.h| header
//     would have to be included in various cc files.
class CORE_EXPORT AuditsIssue {
 public:
  AuditsIssue() = delete;
  AuditsIssue(const AuditsIssue&) = delete;
  AuditsIssue& operator=(const AuditsIssue&) = delete;

  AuditsIssue(AuditsIssue&&);
  AuditsIssue& operator=(AuditsIssue&&);

  const protocol::Audits::InspectorIssue* issue() const { return issue_.get(); }
  std::unique_ptr<protocol::Audits::InspectorIssue> TakeIssue() {
    return std::move(issue_);
  }

  ~AuditsIssue();

 private:
  explicit AuditsIssue(std::unique_ptr<protocol::Audits::InspectorIssue> issue);

  std::unique_ptr<protocol::Audits::InspectorIssue> issue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
