// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/json_sanitizer.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace data_decoder {

// static
void JsonSanitizer::Sanitize(const std::string& json, Callback callback) {
  DataDecoder::ParseJsonIsolated(
      json,
      base::BindOnce(
          [](Callback callback, DataDecoder::ValueOrError parse_result) {
            if (!parse_result.value) {
              std::move(callback).Run(Result::Error(*parse_result.error));
              return;
            }

            const base::Value::Type type = parse_result.value->type();
            if (type != base::Value::Type::DICTIONARY &&
                type != base::Value::Type::LIST) {
              std::move(callback).Run(Result::Error("Invalid top-level type"));
              return;
            }

            std::string safe_json;
            if (!base::JSONWriter::Write(*parse_result.value, &safe_json)) {
              std::move(callback).Run(Result::Error("Encoding error"));
              return;
            }

            Result result;
            result.value = std::move(safe_json);
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

}  // namespace data_decoder
