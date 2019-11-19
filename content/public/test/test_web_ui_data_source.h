// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_WEB_UI_DATA_SOURCE_H_
#define CONTENT_PUBLIC_TEST_TEST_WEB_UI_DATA_SOURCE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/base/template_expressions.h"

namespace base {
class DictionaryValue;
}

namespace content {

class WebUIDataSource;

class TestWebUIDataSource {
 public:
  static std::unique_ptr<TestWebUIDataSource> Create(
      const std::string& source_name);

  virtual ~TestWebUIDataSource() = default;

  virtual const base::DictionaryValue* GetLocalizedStrings() = 0;

  virtual const ui::TemplateReplacements* GetReplacements() = 0;

  virtual WebUIDataSource* GetWebUIDataSource() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_WEB_UI_DATA_SOURCE_H_
