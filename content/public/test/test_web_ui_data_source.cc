// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_web_ui_data_source.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/webui/web_ui_data_source_impl.h"

namespace content {

class WebUIDataSourceImplWithPublicData : public WebUIDataSourceImpl {
 public:
  static WebUIDataSourceImplWithPublicData* Create(
      const std::string& source_name) {
    return new WebUIDataSourceImplWithPublicData(source_name);
  }

  WebUIDataSourceImplWithPublicData(const WebUIDataSourceImplWithPublicData&) =
      delete;
  WebUIDataSourceImplWithPublicData& operator=(
      const WebUIDataSourceImplWithPublicData&) = delete;

  using WebUIDataSourceImpl::GetLocalizedStrings;
  using WebUIDataSourceImpl::URLToIdrOrDefault;

 protected:
  explicit WebUIDataSourceImplWithPublicData(const std::string& source_name)
      : WebUIDataSourceImpl(source_name) {}
  ~WebUIDataSourceImplWithPublicData() override {}
};

class TestWebUIDataSourceImpl : public TestWebUIDataSource {
 public:
  explicit TestWebUIDataSourceImpl(const std::string& source_name)
      : source_(WebUIDataSourceImplWithPublicData::Create(source_name)) {}

  TestWebUIDataSourceImpl(const TestWebUIDataSourceImpl&) = delete;
  TestWebUIDataSourceImpl& operator=(const TestWebUIDataSourceImpl&) = delete;

  ~TestWebUIDataSourceImpl() override {}

  const base::Value::Dict* GetLocalizedStrings() override {
    return source_->GetLocalizedStrings();
  }

  const ui::TemplateReplacements* GetReplacements() override {
    return source_->source()->GetReplacements();
  }

  int URLToIdrOrDefault(const GURL& url) override {
    return source_->URLToIdrOrDefault(url);
  }

  WebUIDataSource* GetWebUIDataSource() override { return source_.get(); }

  void AddDataSourceForBrowserContext(BrowserContext* context) override {
    URLDataManager::AddWebUIDataSource(context, GetWebUIDataSource());
  }

 private:
  scoped_refptr<WebUIDataSourceImplWithPublicData> source_;
};

// static
std::unique_ptr<TestWebUIDataSource> TestWebUIDataSource::Create(
    const std::string& source_name) {
  return std::make_unique<TestWebUIDataSourceImpl>(source_name);
}

}  // namespace content
