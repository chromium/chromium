// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_TRACING_CONTROLLER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_TRACING_CONTROLLER_H_

#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "content/common/content_export.h"

namespace perfetto {
class TracingSession;
}  // namespace perfetto

namespace content {

class WebTestTracingController {
 public:
  explicit WebTestTracingController(base::FilePath trace_file_path);
  ~WebTestTracingController();

  void StartTracing();
  void StopTracing();
  void TracingFinished();

 private:
  void OnTracingStopped();

  base::FilePath trace_file_path_;
  base::File tracing_file_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  std::optional<base::RunLoop> stop_tracing_run_loop_;
  bool tracing_is_stopping_ = false;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_TRACING_CONTROLLER_H_
