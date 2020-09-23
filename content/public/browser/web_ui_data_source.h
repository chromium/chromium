// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class RefCountedMemory;
}  // namespace base

namespace content {
class BrowserContext;

// A data source that can help with implementing the common operations needed by
// WebUI pages.
class WebUIDataSource {
 public:
  virtual ~WebUIDataSource() {}

  CONTENT_EXPORT static WebUIDataSource* Create(const std::string& source_name);

  // Adds a WebUI data source to |browser_context|. TODO(dbeam): update this API
  // to take a std::unique_ptr instead to make it clear that |source| can be
  // destroyed and references should not be kept by callers. Use |Update()|
  // if you need to change an existing data source.
  CONTENT_EXPORT static void Add(BrowserContext* browser_context,
                                 WebUIDataSource* source);

  CONTENT_EXPORT static void Update(
      BrowserContext* browser_context,
      const std::string& source_name,
      std::unique_ptr<base::DictionaryValue> update);

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(base::StringPiece name,
                         const base::string16& value) = 0;

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(base::StringPiece name, const std::string& value) = 0;

  // Adds a localized string with resource |ids| keyed to its name to our
  // dictionary.
  virtual void AddLocalizedString(base::StringPiece name, int ids) = 0;

  // Add strings from |localized_strings| to our dictionary.
  virtual void AddLocalizedStrings(
      const base::DictionaryValue& localized_strings) = 0;

  // Adds a boolean keyed to its name to our dictionary.
  virtual void AddBoolean(base::StringPiece name, bool value) = 0;

  // Adds a signed 32-bit integer keyed to its name to our dictionary. Larger
  // integers may not be exactly representable in JavaScript. See
  // MAX_SAFE_INTEGER in /v8/src/globals.h.
  virtual void AddInteger(base::StringPiece name, int32_t value) = 0;

  // Adds a double keyed to its name  to our dictionary.
  virtual void AddDouble(base::StringPiece name, double value) = 0;

  // Call this to enable a virtual "strings.js" (or "strings.m.js" for modules)
  // URL that provides translations and dynamic data when requested.
  virtual void UseStringsJs() = 0;

  // Adds a mapping between a path name and a resource to return.
  virtual void AddResourcePath(base::StringPiece path, int resource_id) = 0;

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
  // NOTE: it's not acceptable to call DisableContentSecurityPolicy for new
  // pages, see URLDataSource::ShouldAddContentSecurityPolicy and talk to
  // tsepez.

  // Currently only used by embedders for WebUIs with multiple instances.
  virtual void DisableReplaceExistingSource() = 0;
  virtual void DisableContentSecurityPolicy() = 0;

  // Overrides the content security policy for a certain directive.
  virtual void OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive,
      const std::string& value) = 0;

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
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_DATA_SOURCE_H_
