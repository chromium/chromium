// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_page.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/webplugininfo.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace extensions {

namespace {

// TODO(crbug.com/40490789): Make this a proper resource.
constexpr char kFullPageMimeHandlerViewHTML[] =
    "<!doctype html><html><body style='height: 100%%; width: 100%%; overflow: "
    "hidden; margin:0px; background-color: rgb(%d, %d, %d);'><embed "
    "name='%s' "
    "style='position:absolute; left: 0; top: 0;'width='100%%' height='100%%'"
    " src='about:blank' type='%s' "
    "internalid='%s'></body></html>";

// Generic iframe-based template for Generic MIME handlers
constexpr char kOopifMimeHandlerViewHTML[] =
    "<!doctype html><html style='height:100%%;width:100%%'><body "
    "style='height:100%%;width:100%%;overflow:hidden;margin:0;padding:0;"
    "%s'>"
    "<template shadowrootmode='closed'>"
    "<iframe name='%s' src='about:blank' type='%s' internalid='%s' "
    "style='border:0;position:absolute;top:0;left:0;width:100%%;height:100%%' "
    "allow='fullscreen *'>"
    "</iframe><slot></slot></template></body></html>";

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
    return base::StringPrintf(kOopifMimeHandlerViewHTML, maybe_color,
                              internal_id.c_str(), mime_type.c_str(),
                              internal_id.c_str());
  }
  return base::StringPrintf(kFullPageMimeHandlerViewHTML, SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color),
                            internal_id.c_str(), mime_type.c_str(),
                            internal_id.c_str());
}

}  // namespace extensions
