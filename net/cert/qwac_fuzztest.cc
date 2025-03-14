// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "net/cert/qwac.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace net {

namespace {

bool IsSubSpan(base::span<const uint8_t> inner,
               base::span<const uint8_t> outer) {
  if (inner.empty()) {
    return true;
  }
  if (outer.empty()) {
    return false;
  }
  return ((inner.data() >= outer.data()) && (&inner.back() <= &outer.back()));
}

void FuzzParseQcStatements(std::vector<uint8_t> extension_value) {
  const auto parsed = ParseQcStatements(bssl::der::Input(extension_value));
  if (!parsed.has_value()) {
    return;
  }
  for (const auto& statement : parsed.value()) {
    ASSERT_TRUE(IsSubSpan(statement.id, extension_value));
    ASSERT_TRUE(IsSubSpan(statement.info, extension_value));
  }
}

constexpr uint8_t kEmptySequence[] = {0x30, 0x0};

// SEQUENCE { OBJECT_IDENTIFIER { 1.2.3 } }
constexpr uint8_t kInvalidStatementSequence[] = {0x30, 0x04, 0x06,
                                                 0x02, 0x2a, 0x03};

// SEQUENCE { SEQUENCE { SEQUENCE { OBJECT_IDENTIFIER { 1.2.4 } } } }
constexpr uint8_t kInvalidStatementOid[] = {0x30, 0x08, 0x30, 0x06, 0x30,
                                            0x04, 0x06, 0x02, 0x2a, 0x04};

// SEQUENCE {
//   SEQUENCE {
//     OBJECT_IDENTIFIER { 1.3.6.1.5.5.7.11.2 }
//     SEQUENCE {
//       OBJECT_IDENTIFIER { 0.4.0.194121.1.2 }
//     }
//   }
//   SEQUENCE {
//     OBJECT_IDENTIFIER { 0.4.0.1862.1.1 }
//   }
//   SEQUENCE {
//     OBJECT_IDENTIFIER { 0.4.0.1862.1.6 }
//     SEQUENCE {
//       OBJECT_IDENTIFIER { 0.4.0.1862.1.6.3 }
//     }
//   }
// }
constexpr uint8_t kQcStatementsValue[] = {
    0x30, 0x36, 0x30, 0x15, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
    0x0b, 0x02, 0x30, 0x09, 0x06, 0x07, 0x04, 0x00, 0x8b, 0xec, 0x49, 0x01,
    0x02, 0x30, 0x08, 0x06, 0x06, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x01, 0x30,
    0x13, 0x06, 0x06, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x06, 0x30, 0x09, 0x06,
    0x07, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x06, 0x03};

FUZZ_TEST(QwacFuzzTest, FuzzParseQcStatements)
    .WithSeeds({
        base::ToVector(kEmptySequence),
        base::ToVector(kInvalidStatementSequence),
        base::ToVector(kInvalidStatementOid),
        base::ToVector(kQcStatementsValue),
    });

void FuzzHasQwacQcStatements(
    const std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>&
        qc_statements_input) {
  std::vector<QcStatement> qc_statements;
  for (const auto& statement : qc_statements_input) {
    qc_statements.emplace_back(bssl::der::Input(statement.first),
                               bssl::der::Input(statement.second));
  }
  std::ignore = HasQwacQcStatements(qc_statements);
}

// TODO(crbug.com/392931068): Use initial seeds and/or seeded domains?
// TODO(crbug.com/392931068): not sure how useful this fuzzer actually is.
// Maybe refactor things (extract the QcType parsing into a separate function?)
// so that the fuzzer can be more focused?
FUZZ_TEST(QwacFuzzTest, FuzzHasQwacQcStatements);

}  // namespace

}  // namespace net
