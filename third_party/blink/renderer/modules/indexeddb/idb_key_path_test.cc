/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

void CheckKeyPath(const String& key_path,
                  const Vector<String>& expected,
                  int parser_error) {
  IDBKeyPath idb_key_path(key_path);
  ASSERT_EQ(idb_key_path.GetType(), IDBKeyPath::kStringType);
  ASSERT_EQ(idb_key_path.IsValid(),
            (parser_error == kIDBKeyPathParseErrorNone));

  IDBKeyPathParseError error;
  Vector<String> key_path_elements;
  IDBParseKeyPath(key_path, key_path_elements, error);
  ASSERT_EQ(parser_error, error);
  if (error != kIDBKeyPathParseErrorNone)
    return;
  ASSERT_EQ(expected.size(), key_path_elements.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i)
    ASSERT_TRUE(expected[i] == key_path_elements[i]) << i;
}

TEST(IDBKeyPathTest, ValidKeyPath0) {
  Vector<String> expected;
  String key_path("");
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorNone);
}

TEST(IDBKeyPathTest, ValidKeyPath1) {
  Vector<String> expected;
  String key_path("foo");
  expected.push_back(String("foo"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorNone);
}

TEST(IDBKeyPathTest, ValidKeyPath2) {
  Vector<String> expected;
  String key_path("foo.bar.baz");
  expected.push_back(String("foo"));
  expected.push_back(String("bar"));
  expected.push_back(String("baz"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorNone);
}

TEST(IDBKeyPathTest, InvalidKeyPath0) {
  Vector<String> expected;
  String key_path(" ");
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

TEST(IDBKeyPathTest, InvalidKeyPath1) {
  Vector<String> expected;
  String key_path("+foo.bar.baz");
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

TEST(IDBKeyPathTest, InvalidKeyPath2) {
  Vector<String> expected;
  String key_path("foo bar baz");
  expected.push_back(String("foo"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

TEST(IDBKeyPathTest, InvalidKeyPath3) {
  Vector<String> expected;
  String key_path("foo .bar .baz");
  expected.push_back(String("foo"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

TEST(IDBKeyPathTest, InvalidKeyPath4) {
  Vector<String> expected;
  String key_path("foo. bar. baz");
  expected.push_back(String("foo"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

TEST(IDBKeyPathTest, InvalidKeyPath5) {
  Vector<String> expected;
  String key_path("foo..bar..baz");
  expected.push_back(String("foo"));
  CheckKeyPath(key_path, expected, kIDBKeyPathParseErrorIdentifier);
}

}  // namespace
}  // namespace blink
