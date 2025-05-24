// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/send_result.h"

#include <variant>

#include "base/functional/overloaded.h"

namespace content {

SendResult::Status SendResult::status() const {
  return std::visit(
      base::Overloaded{[](Sent sent) {
                         switch (sent.result) {
                           case Sent::Result::kSent:
                             return Status::kSent;
                           case Sent::Result::kTransientFailure:
                             return Status::kTransientFailure;
                           case Sent::Result::kFailure:
                             return Status::kFailure;
                         }
                       },
                       [](Dropped) { return Status::kDropped; },
                       [](Expired) { return Status::kExpired; },
                       [](AssemblyFailure failure) {
                         return failure.transient
                                    ? Status::kTransientAssemblyFailure
                                    : Status::kAssemblyFailure;
                       }},
      result);
}

}  // namespace content
