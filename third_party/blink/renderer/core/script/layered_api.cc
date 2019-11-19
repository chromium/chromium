// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/layered_api.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/script/layered_api_resources.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace layered_api {

namespace {

static const char kStdScheme[] = "std";
static const char kInternalScheme[] = "std-internal";

constexpr char kTopLevelScriptPostfix[] = "/index.mjs";

const LayeredAPIResource* GetResourceFromPath(const Modulator& modulator,
                                              const String& path) {
  for (size_t i = 0; i < base::size(kLayeredAPIResources); ++i) {
    if (modulator.BuiltInModuleEnabled(kLayeredAPIResources[i].module) &&
        path == kLayeredAPIResources[i].path) {
      return &kLayeredAPIResources[i];
    }
  }
  return nullptr;
}

bool IsImplemented(const Modulator& modulator, const String& name) {
  return GetResourceFromPath(modulator, name + kTopLevelScriptPostfix);
}

}  // namespace

String GetBuiltinPath(const KURL& url) {
  if (url.ProtocolIs(kStdScheme))
    return url.GetPath();

  return String();
}

// https://github.com/drufball/layered-apis/blob/master/spec.md#user-content-layered-api-fetching-url
KURL ResolveFetchingURL(const Modulator& modulator, const KURL& url) {
  // <spec step="1">If url's scheme is not "std", return url.</spec>
  // <spec step="2">Let path be url's path[0].</spec>
  String path = GetBuiltinPath(url);
  if (path.IsNull())
    return url;

  // <spec step="5">If the layered API identified by path is implemented by this
  // user agent, return the result of parsing the concatenation of "std:" with
  // identifier.</spec>
  if (IsImplemented(modulator, path)) {
    StringBuilder url_string;
    url_string.Append(kStdScheme);
    url_string.Append(":");
    url_string.Append(path);
    return KURL(NullURL(), url_string.ToString());
  }

  return NullURL();
}

KURL GetInternalURL(const KURL& url) {
  String path = GetBuiltinPath(url);
  if (!path.IsNull()) {
    StringBuilder url_string;
    url_string.Append(kInternalScheme);
    url_string.Append("://");
    url_string.Append(path);
    url_string.Append(kTopLevelScriptPostfix);
    return KURL(NullURL(), url_string.ToString());
  }

  if (url.ProtocolIs(kInternalScheme)) {
    return url;
  }

  return NullURL();
}

String GetSourceText(const Modulator& modulator, const KURL& url) {
  if (!url.ProtocolIs(kInternalScheme))
    return String();

  String path = url.GetPath();
  // According to the URL spec, the host/path of "std-internal://foo/bar"
  // is "foo" and "/bar", respectively, but in Blink they are "" and
  // "//foo/bar". This is a workaround to get "foo/bar" here.
  if (path.StartsWith("//")) {
    path = path.Substring(2);
  }

  const LayeredAPIResource* resource = GetResourceFromPath(modulator, path);
  if (!resource)
    return String();

  // Only count the use of top-level scripts of each built-in module.
  if (path.EndsWith(kTopLevelScriptPostfix))
    modulator.BuiltInModuleUseCount(resource->module);

  return UncompressResourceAsString(resource->resource_id);
}

}  // namespace layered_api

}  // namespace blink
