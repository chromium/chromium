// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_V1_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_V1_H_

#include <fuchsia/sys/cpp/fidl.h>

// fuchsia.sys.Runner implementation which delegates hosting of cast/casts
// activities to the actual CFv2-based runner.
class CastRunnerV1 final : public fuchsia::sys::Runner {
 public:
  CastRunnerV1();
  ~CastRunnerV1() override;

  CastRunnerV1(const CastRunnerV1&) = delete;
  CastRunnerV1& operator=(const CastRunnerV1&) = delete;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_V1_H_
