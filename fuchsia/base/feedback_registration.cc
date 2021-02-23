// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/feedback_registration.h"

#include <fuchsia/feedback/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "base/strings/string_piece.h"
#include "components/version_info/version_info.h"

namespace cr_fuchsia {

void RegisterProductDataForCrashReporting(
    base::StringPiece component_url,
    base::StringPiece crash_product_name) {
  fuchsia::feedback::CrashReportingProduct product_data;
  product_data.set_name(std::string(crash_product_name));
  product_data.set_version(version_info::GetVersionNumber());
  // TODO(https://crbug.com/1077428): Use the actual channel when appropriate.
  // For now, always set it to the empty string to avoid reporting "missing".
  product_data.set_channel("");
  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::feedback::CrashReportingProductRegister>()
      ->Upsert(std::string(component_url), std::move(product_data));
}

void RegisterProductDataForFeedback(base::StringPiece component_namespace) {
  fuchsia::feedback::ComponentData component_data;
  component_data.set_namespace_(std::string(component_namespace));
  // TODO(https://crbug.com/1077428): Add release channel to the annotations.
  component_data.mutable_annotations()->push_back(
      {"version", version_info::GetVersionNumber()});
  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::feedback::ComponentDataRegister>()
      ->Upsert(std::move(component_data), []() {});
}

}  // namespace cr_fuchsia
