// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/layered_api.h"

#include "base/stl_util.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace layered_api {

namespace {

static const char kStdScheme[] = "std";
static const char kInternalScheme[] = "std-internal";

struct LayeredAPIResource {
  const char* path;
  int resource_id;
};

const LayeredAPIResource kLayeredAPIResources[] = {
    {"blank/index.js", IDR_LAYERED_API_BLANK_INDEX_JS},

    {"async-local-storage/index.js",
     IDR_LAYERED_API_ASYNC_LOCAL_STORAGE_INDEX_JS},

    {"virtual-scroller/index.js", IDR_LAYERED_API_VIRTUAL_SCROLLER_INDEX_JS},
    {"virtual-scroller/item-source.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_ITEM_SOURCE_JS},
    {"virtual-scroller/layouts/layout-1d-base.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_LAYOUTS_LAYOUT_1D_BASE_JS},
    {"virtual-scroller/layouts/layout-1d-grid.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_LAYOUTS_LAYOUT_1D_GRID_JS},
    {"virtual-scroller/layouts/layout-1d.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_LAYOUTS_LAYOUT_1D_JS},
    {"virtual-scroller/virtual-scroller.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_VIRTUAL_SCROLLER_JS},
    {"virtual-scroller/virtual-repeater.js",
     IDR_LAYERED_API_VIRTUAL_SCROLLER_VIRTUAL_REPEATER_JS},
};

int GetResourceIDFromPath(const String& path) {
  for (size_t i = 0; i < base::size(kLayeredAPIResources); ++i) {
    if (path == kLayeredAPIResources[i].path) {
      return kLayeredAPIResources[i].resource_id;
    }
  }
  return -1;
}

bool IsImplemented(const String& name) {
  return GetResourceIDFromPath(name + "/index.js") >= 0;
}

}  // namespace

// https://github.com/drufball/layered-apis/blob/master/spec.md#user-content-layered-api-fetching-url
KURL ResolveFetchingURL(const KURL& url) {
  // <spec step="1">If url's scheme is not "std", return url.</spec>
  if (!url.ProtocolIs(kStdScheme))
    return url;

  // <spec step="2">Let path be url's path[0].</spec>
  const String path = url.GetPath();

  // <spec step="5">If the layered API identified by path is implemented by this
  // user agent, return the result of parsing the concatenation of "std:" with
  // identifier.</spec>
  if (IsImplemented(path)) {
    StringBuilder url_string;
    url_string.Append(kStdScheme);
    url_string.Append(":");
    url_string.Append(path);
    return KURL(NullURL(), url_string.ToString());
  }

  return NullURL();
}

KURL GetInternalURL(const KURL& url) {
  if (url.ProtocolIs(kStdScheme)) {
    StringBuilder url_string;
    url_string.Append(kInternalScheme);
    url_string.Append("://");
    url_string.Append(url.GetPath());
    url_string.Append("/index.js");
    return KURL(NullURL(), url_string.ToString());
  }

  if (url.ProtocolIs(kInternalScheme)) {
    return url;
  }

  return NullURL();
}

String GetSourceText(const KURL& url) {
  if (!url.ProtocolIs(kInternalScheme))
    return String();

  String path = url.GetPath();
  // According to the URL spec, the host/path of "std-internal://foo/bar"
  // is "foo" and "/bar", respectively, but in Blink they are "" and
  // "//foo/bar". This is a workaround to get "foo/bar" here.
  if (path.StartsWith("//")) {
    path = path.Substring(2);
  }

  int resource_id = GetResourceIDFromPath(path);
  if (resource_id < 0)
    return String();

  return GetResourceAsString(resource_id);
}

}  // namespace layered_api

}  // namespace blink
