// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"

#import "base/strings/utf_string_conversions.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "ui/base/resource/resource_bundle.h"

UIImage* SearchEngineFaviconFromTemplateURL(const TemplateURL& template_url) {
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0, base::NotFatalUntil::M127)
      << base::UTF16ToUTF8(template_url.short_name());
  std::u16string engine_keyword = template_url.data().keyword();
  int resource_id = search_engines::GetIconResourceId(engine_keyword);
  if (resource_id == -1) {
    // It is possible to have no resource id for a prepopulated search engine
    // that was selected from a country outside of EEA countries.
    return [UIImage imageNamed:@"default_world_favicon"];
  }
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  return resource_bundle.GetNativeImageNamed(resource_id).ToUIImage();
}
