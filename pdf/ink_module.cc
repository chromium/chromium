// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "pdf/pdf_features.h"

namespace chrome_pdf {

InkModule::InkModule() {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
}

InkModule::~InkModule() = default;

bool InkModule::OnMessage(const base::Value::Dict& message) {
  using MessageHandler = void (InkModule::*)(const base::Value::Dict&);

  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<std::string_view, MessageHandler>({
          {"setAnnotationMode", &InkModule::HandleSetAnnotationModeMessage},
      });

  auto it = kMessageHandlers.find(*message.FindString("type"));
  if (it == kMessageHandlers.end()) {
    return false;
  }

  MessageHandler handler = it->second;
  (this->*handler)(message);
  return true;
}

void InkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
}

}  // namespace chrome_pdf
