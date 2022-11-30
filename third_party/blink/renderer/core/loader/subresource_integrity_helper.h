// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_INTEGRITY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_INTEGRITY_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT SubresourceIntegrityHelper final {
  STATIC_ONLY(SubresourceIntegrityHelper);

 public:
  static void DoReport(ExecutionContext&,
                       const SubresourceIntegrity::ReportInfo&);

  static void GetConsoleMessages(const SubresourceIntegrity::ReportInfo&,
                                 HeapVector<Member<ConsoleMessage>>*);

  static SubresourceIntegrity::IntegrityFeatures GetFeatures(ExecutionContext*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_INTEGRITY_HELPER_H_
