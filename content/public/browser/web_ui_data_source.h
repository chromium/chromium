// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace webui {
struct LocalizedString;
struct ResourcePath;
}  // namespace webui

namespace content {
class BrowserContext;

// A data source that can help with implementing the common operations needed by
// WebUI pages.
class WebUIDataSource {
 public:
  virtual ~WebUIDataSource() {}

  // Creates a WebUIDataSource and adds it to the BrowserContext, which owns it.
  // Callers just get a raw pointer, which they don't own.
  // `source_name` is the key for URL lookups, allowing the source to serve
  // content for URLs that match the patterns:
  //  - chrome://source_name/*
  //  - chrome-untrusted://<host>/*
  //    (source_name is of the form "chrome-untrusted://<host>")
  //  - scheme://*
  //    (source_name is of the form "scheme://")
  CONTENT_EXPORT static WebUIDataSource* CreateAndAdd(
      BrowserContext* browser_context,
      const std::string& source_name);

  CONTENT_EXPORT static void Update(BrowserContext* browser_context,
                                    const std::string& source_name,
                                    const base::Value::Dict& update);

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(std::string_view name, std::u16string_view value) = 0;

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(std::string_view name, std::string_view value) = 0;

  // Adds a localized string with resource |ids| keyed to its name to our
  // dictionary.
  virtual void AddLocalizedString(std::string_view name, int ids) = 0;

  // Calls AddLocalizedString() in a for-loop for |strings|. Reduces code size
  // vs. reimplementing the same for-loop.
  virtual void AddLocalizedStrings(
      base::span<const webui::LocalizedString> strings) = 0;

  // Add strings from `localized_strings` to our dictionary.
  virtual void AddLocalizedStrings(
      const base::Value::Dict& localized_strings) = 0;

  // Adds a boolean keyed to its name to our dictionary.
  virtual void AddBoolean(std::string_view name, bool value) = 0;

  // Adds a signed 32-bit integer keyed to its name to our dictionary. Larger
  // integers may not be exactly representable in JavaScript. See
  // MAX_SAFE_INTEGER in /v8/src/globals.h.
  virtual void AddInteger(std::string_view name, int32_t value) = 0;

  // Adds a double keyed to its name  to our dictionary.
  virtual void AddDouble(std::string_view name, double value) = 0;

  // Call this to enable a virtual "strings.js" (or "strings.m.js" for modules)
  // URL that provides translations and dynamic data when requested.
  virtual void UseStringsJs() = 0;

  // Adds a mapping between a path name and a resource to return.
  virtual void AddResourcePath(std::string_view path, int resource_id) = 0;

  // Calls AddResourcePath() in a for-loop for |paths|. Reduces code size vs.
  // reimplementing the same for-loop.
  virtual void AddResourcePaths(
      base::span<const webui::ResourcePath> paths) = 0;

  // Sets the resource to returned when no other paths match.
  virtual void SetDefaultResource(int resource_id) = 0;

  // Used as a parameter to GotDataCallback. The caller has to run this callback
  // with the result for the path that they filtered, passing ownership of the
  // memory.
  using GotDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

  // Used by SetRequestFilter. The string parameter is the path of the request.
  // The return value indicates if the callee wants to handle the request. Iff
  // true is returned, |handle_request_callback| will be called to provide the
  // request's response.
  typedef base::RepeatingCallback<bool(const std::string&)>
      ShouldHandleRequestCallback;

  // Used by SetRequestFilter. The string parameter is the path of the request.
  // This callback is only called if a prior call to ShouldHandleRequestCallback
  // returned true. GotDataCallback should be used to provide the response
  // bytes.
  using HandleRequestCallback =
      base::RepeatingCallback<void(const std::string&, GotDataCallback)>;

  // Allows a caller to add a filter for URL requests.
  virtual void SetRequestFilter(
      const ShouldHandleRequestCallback& should_handle_request_callback,
      const HandleRequestCallback& handle_request_callback) = 0;

  // The following map to methods on URLDataSource. See the documentation there.
  // Overrides the content security policy for a certain directive.
  virtual void OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive,
      const std::string& value) = 0;

  // Using OverrideCrossOriginOpenerPolicy will result in the creation of double
  // WebUIControllers. See https://crbug.com/328741392. Until this bug is fixed,
  // usage of this API is discouraged.
  // Adds cross origin opener, embedder, and resource policy headers.
  virtual void OverrideCrossOriginOpenerPolicy(const std::string& value) = 0;
  virtual void OverrideCrossOriginEmbedderPolicy(const std::string& value) = 0;
  virtual void OverrideCrossOriginResourcePolicy(const std::string& value) = 0;

  // Removes directives related to Trusted Types from the CSP header.
  virtual void DisableTrustedTypesCSP() = 0;

  // This method is deprecated and AddFrameAncestors should be used instead.
  virtual void DisableDenyXFrameOptions() = 0;
  virtual void AddFrameAncestor(const GURL& frame_ancestor) = 0;

  // Replace i18n template strings in JS files. Needed for Web UIs that are
  // using Polymer 3.
  virtual void EnableReplaceI18nInJS() = 0;

  // The |source_name| this WebUIDataSource was created with.
  virtual std::string GetSource() = 0;

  // The scheme of URLs served by this data source.
  virtual std::string GetScheme() = 0;

  // Set supported scheme if not one of the default supported schemes.
  virtual void SetSupportedScheme(std::string_view scheme) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
