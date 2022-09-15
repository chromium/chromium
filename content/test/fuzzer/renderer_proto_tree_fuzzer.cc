// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for content/renderer

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <sstream>

#include "content/test/fuzzer/fuzzer_support.h"
#include "content/test/fuzzer/html_tree.pb.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace content {

class HtmlTreeWriter {
 public:
  HtmlTreeWriter() {}

  template <typename T>
  HtmlTreeWriter& operator<<(const T& t) {
    out_ << t;
    return *this;
  }

  std::string str() const { return out_.str(); }

 private:
  std::ostringstream out_;
};

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w,
                                  const Attribute::Value& value) {
  switch (value.value_case()) {
    case Attribute::Value::kBoolValue:
      return w << (value.bool_value() ? "true" : "false");
    case Attribute::Value::kUintValue:
      return w << value.uint_value();
    case Attribute::Value::kIntValue:
      return w << value.int_value();
    case Attribute::Value::kDoubleValue:
      return w << value.double_value();
    case Attribute::Value::kPxValue:
      return w << value.px_value() << "px";
    case Attribute::Value::kPctValue:
      return w << value.pct_value() << "%";
    case Attribute::Value::VALUE_NOT_SET:
      return w;
  }
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w,
                                  const Attribute::Name& name) {
  return w << Attribute_Name_Name(name);
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w, const Attribute& attr) {
  return w << attr.name() << "=\"" << attr.value() << "\"";
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w, const Tag::Name& tagName) {
  return w << Tag_Name_Name(tagName);
}

static void operator<<(HtmlTreeWriter& w, const Tag& tag) {
  w << "<" << tag.name();
  for (const auto& attr : tag.attrs()) {
    w << " " << attr;
  }

  w << ">";
  for (const auto& subtag : tag.subtags()) {
    w << subtag;
  }
  w << "</" << tag.name() << ">";
}

static void operator<<(HtmlTreeWriter& w, const Document& document) {
  w << document.root();
}

static std::string str(const Document& document) {
  HtmlTreeWriter writer;
  writer << document;
  return writer.str();
}

static Env* env = nullptr;

DEFINE_BINARY_PROTO_FUZZER(const Document& document) {
  // Environment has to be initialized in the same thread.
  if (env == nullptr)
    env = new Env();

  env->adapter->LoadHTML(str(document), "http://www.example.org");
}

}  // namespace content
