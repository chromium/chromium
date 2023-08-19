// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/shared_resources_data_source_ios.h"

#import <stddef.h>

#import "base/check.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/string_util.h"
#import "ios/web/grit/ios_web_resources.h"
#import "ios/web/grit/ios_web_resources_map.h"
#import "ios/web/public/web_client.h"
#import "mojo/public/js/grit/mojo_bindings_resources.h"
#import "mojo/public/js/grit/mojo_bindings_resources_map.h"
#import "net/base/mime_util.h"
#import "ui/base/webui/resource_path.h"
#import "ui/base/webui/web_ui_util.h"
#import "ui/resources/grit/webui_resources.h"
#import "ui/resources/grit/webui_resources_map.h"

namespace web {

namespace {

// Value duplicated from content/public/common/url_constants.h
// TODO(stuartmorgan): Revisit how to share this in a more maintainable way.
const char kWebUIResourcesHost[] = "resources";

// Maps a path name (i.e. "/js/path.js") to a resource map entry. Returns
// nullptr if not found.
const webui::ResourcePath* PathToResource(const std::string& path) {
  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    if (path == kWebuiResources[i].path) {
      return &kWebuiResources[i];
    }
  }
  for (size_t i = 0; i < kMojoBindingsResourcesSize; ++i) {
    if (path == kMojoBindingsResources[i].path)
      return &kMojoBindingsResources[i];
  }
  for (size_t i = 0; i < kIosWebResourcesSize; ++i) {
    if (path == kIosWebResources[i].path)
      return &kIosWebResources[i];
  }

  return nullptr;
}

}  // namespace

SharedResourcesDataSourceIOS::SharedResourcesDataSourceIOS() {}

SharedResourcesDataSourceIOS::~SharedResourcesDataSourceIOS() {}

std::string SharedResourcesDataSourceIOS::GetSource() const {
  return kWebUIResourcesHost;
}

void SharedResourcesDataSourceIOS::StartDataRequest(
    const std::string& path,
    URLDataSourceIOS::GotDataCallback callback) {
  const webui::ResourcePath* resource = PathToResource(path);
  DCHECK(resource) << " path: " << path;
  scoped_refptr<base::RefCountedMemory> bytes;

  WebClient* web_client = GetWebClient();

  int idr = resource ? resource->id : -1;
  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS_CSS) {
    bytes = base::MakeRefCounted<base::RefCountedString>(
        webui::GetWebUiCssTextDefaults());
  } else {
    bytes = web_client->GetDataResourceBytes(idr);
  }

  std::move(callback).Run(bytes.get());
}

std::string SharedResourcesDataSourceIOS::GetMimeType(
    const std::string& path) const {
  std::string mime_type;
  net::GetMimeTypeFromFile(base::FilePath().AppendASCII(path), &mime_type);
  return mime_type;
}

}  // namespace web
