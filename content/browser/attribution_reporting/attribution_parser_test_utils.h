// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PARSER_TEST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PARSER_TEST_UTILS_H_

#include <stddef.h>

#include <iosfwd>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

class AttributionParserErrorManager {
 public:
  using Context = absl::variant<base::StringPiece, size_t>;
  using ContextPath = std::vector<Context>;

  explicit AttributionParserErrorManager(std::ostream& stream);
  ~AttributionParserErrorManager();

  AttributionParserErrorManager(const AttributionParserErrorManager&) = delete;
  AttributionParserErrorManager(AttributionParserErrorManager&&) = delete;

  AttributionParserErrorManager& operator=(
      const AttributionParserErrorManager&) = delete;
  AttributionParserErrorManager& operator=(AttributionParserErrorManager&&) =
      delete;

  class ScopedContext {
   public:
    ScopedContext(ContextPath& path, Context context);

    ~ScopedContext();

    ScopedContext(const ScopedContext&) = delete;
    ScopedContext(ScopedContext&&) = delete;

    ScopedContext& operator=(const ScopedContext&) = delete;
    ScopedContext& operator=(ScopedContext&&) = delete;

   private:
    const raw_ref<ContextPath> path_;
  };

  // Writes a newline on destruction.
  class ErrorWriter {
   public:
    explicit ErrorWriter(std::ostream& stream);

    ~ErrorWriter();

    ErrorWriter(const ErrorWriter&) = delete;
    ErrorWriter(ErrorWriter&&) = default;

    ErrorWriter& operator=(const ErrorWriter&) = delete;
    ErrorWriter& operator=(ErrorWriter&&) = delete;

    std::ostream& operator*();

    void operator()(base::StringPiece key);

    void operator()(size_t index);

   private:
    std::ostream& stream_;
  };

  [[nodiscard]] std::unique_ptr<ScopedContext> PushContext(Context context);

  ErrorWriter Error();

  bool has_error() const { return has_error_; }

  void ResetErrorState() { has_error_ = false; }

 private:
  const raw_ref<std::ostream> error_stream_;

  ContextPath context_path_;
  bool has_error_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PARSER_TEST_UTILS_H_
