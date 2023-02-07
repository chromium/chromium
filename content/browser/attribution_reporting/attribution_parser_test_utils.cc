// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"

#include <stddef.h>

#include <memory>
#include <sstream>
#include <string>

#include "base/strings/string_piece.h"

namespace content {

AttributionParserErrorManager::AttributionParserErrorManager() = default;

AttributionParserErrorManager::~AttributionParserErrorManager() = default;

AttributionParserErrorManager::ScopedContext::ScopedContext(ContextPath& path,
                                                            Context context)
    : path_(path) {
  path_->push_back(context);
}

AttributionParserErrorManager::ScopedContext::~ScopedContext() {
  path_->pop_back();
}

AttributionParserErrorManager::ErrorWriter::ErrorWriter(
    std::ostringstream& stream)
    : stream_(stream) {}

AttributionParserErrorManager::ErrorWriter::~ErrorWriter() {
  *stream_ << std::endl;
}

void AttributionParserErrorManager::ErrorWriter::operator()(
    base::StringPiece key) {
  *stream_ << "[\"" << key << "\"]";
}

void AttributionParserErrorManager::ErrorWriter::operator()(size_t index) {
  *stream_ << '[' << index << ']';
}

std::unique_ptr<AttributionParserErrorManager::ScopedContext>
AttributionParserErrorManager::PushContext(Context context) {
  return std::make_unique<ScopedContext>(context_path_, context);
}

AttributionParserErrorManager::ErrorWriter
AttributionParserErrorManager::Error() {
  has_error_ = true;

  if (context_path_.empty()) {
    error_stream_ << "input root";
  }

  ErrorWriter writer(error_stream_);
  for (Context context : context_path_) {
    absl::visit(writer, context);
  }

  error_stream_ << ": ";
  return writer;
}

std::string AttributionParserErrorManager::TakeError() && {
  return error_stream_.str();
}

}  // namespace content
