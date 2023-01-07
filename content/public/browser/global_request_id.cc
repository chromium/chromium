// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_request_id.h"

#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

void GlobalRequestID::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("child_id", child_id);
  dict.Add("request_id", request_id);
}

}  // namespace content
