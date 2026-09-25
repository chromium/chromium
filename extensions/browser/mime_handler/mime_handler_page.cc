// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_page.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/grit/extensions_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace extensions {

namespace {

SkColor GetBackgroundColorStringForMimeType(const GURL& url,
                                            const std::string& mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  std::vector<content::WebPluginInfo> web_plugin_info_array;
  std::vector<std::string> unused_actual_mime_types;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, &web_plugin_info_array, &unused_actual_mime_types);
  if (!web_plugin_info_array.empty()) {
    return web_plugin_info_array.front().background_color;
  }
#endif
  return content::WebPluginInfo::kDefaultBackgroundColor;
}

}  // namespace

std::string CreateTemplateMimeHandlerPage(const GURL& resource_url,
                                          const std::string& mime_type,
                                          const std::string& internal_id,
                                          bool use_oopif) {
  SkColor color = GetBackgroundColorStringForMimeType(resource_url, mime_type);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          use_oopif ? IDR_OOPIF_MIME_HANDLER_HTML
                    : IDR_FULL_PAGE_MIME_HANDLER_HTML);
  if (use_oopif) {
    std::string maybe_color;
    // This background color comes from a hard-coded list of plugins in
    // MimeTypesHandler::GetBackgroundColor(), so third-party plugins cannot set
    // the color. For those third-party plugins, don't set the background color.
    if (color != content::WebPluginInfo::kDefaultBackgroundColor) {
      maybe_color = base::StringPrintf("background-color:rgb(%d,%d,%d);",
                                       SkColorGetR(color), SkColorGetG(color),
                                       SkColorGetB(color));
    }

    return base::ReplaceStringPlaceholders(
        html, {maybe_color, internal_id, mime_type, internal_id},
        /*offsets=*/nullptr);
  }
  return base::ReplaceStringPlaceholders(
      html,
      {base::StringPrintf("rgb(%d,%d,%d)", SkColorGetR(color),
                          SkColorGetG(color), SkColorGetB(color)),
       internal_id, mime_type, internal_id},
      /*offsets=*/nullptr);
}

}  // namespace extensions
