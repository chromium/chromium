// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_PROTOCOL_STRING_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_PROTOCOL_STRING_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "headless/public/headless_export.h"

namespace base {
class Value;
}

namespace headless {
namespace protocol {

class Value;

using String = std::string;

class HEADLESS_EXPORT StringBuilder {
 public:
  StringBuilder();
  ~StringBuilder();
  void append(const String&);
  void append(char);
  void append(const char*, size_t);
  String toString();
  void reserveCapacity(size_t);

 private:
  std::string string_;
};

class HEADLESS_EXPORT StringUtil {
 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substr(pos, len);
  }
  static String fromInteger(int number) { return base::IntToString(number); }
  static String fromDouble(double number) {
    String s = base::NumberToString(number);
    if (!s.empty() && s[0] == '.')
      s = "0" + s;
    return s;
  }
  static double toDouble(const char* s, size_t len, bool* ok) {
    double v = 0.0;
    *ok = base::StringToDouble(std::string(s, len), &v);
    return *ok ? v : 0.0;
  }
  static size_t find(const String& s, const char* needle) {
    return s.find(needle);
  }
  static size_t find(const String& s, const String& needle) {
    return s.find(needle);
  }
  static const size_t kNotFound = static_cast<size_t>(-1);
  static void builderAppend(StringBuilder& builder, const String& s) {
    builder.append(s);
  }
  static void builderAppend(StringBuilder& builder, char c) {
    builder.append(c);
  }
  static void builderAppend(StringBuilder& builder, const char* s, size_t len) {
    builder.append(s, len);
  }
  static void builderAppendQuotedString(StringBuilder& builder,
                                        const String& str);
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static String builderToString(StringBuilder& builder) {
    return builder.toString();
  }

  static std::unique_ptr<Value> parseJSON(const String&);
};

// A read-only sequence of uninterpreted bytes with reference-counted storage.
class HEADLESS_EXPORT Binary {
 public:
  Binary(const Binary&);
  Binary();
  ~Binary();

  const uint8_t* data() const { return bytes_->front(); }
  size_t size() const { return bytes_->size(); }

  String toBase64() const;
  static Binary fromBase64(const String& base64, bool* success);
  static Binary fromRefCounted(scoped_refptr<base::RefCountedMemory> memory);
  static Binary fromVector(std::vector<uint8_t> data);
  static Binary fromString(std::string data);

 private:
  explicit Binary(scoped_refptr<base::RefCountedMemory> bytes);
  scoped_refptr<base::RefCountedMemory> bytes_;
};

std::unique_ptr<Value> toProtocolValue(const base::Value* value, int depth);
std::unique_ptr<base::Value> toBaseValue(Value* value, int depth);

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_PROTOCOL_STRING_H_
