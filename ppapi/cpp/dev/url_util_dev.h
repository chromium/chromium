// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_URL_UTIL_DEV_H_
#define PPAPI_CPP_DEV_URL_UTIL_DEV_H_

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/cpp/var.h"

namespace pp {

class InstanceHandle;

// Simple wrapper around the PPB_URLUtil interface.
class URLUtil_Dev {
 public:
  // This class is just a collection of random functions that aren't
  // particularly attached to anything. This may return NULL if the
  // browser doesn't support the URLUtil interface. Since this is a
  // singleton, don't delete the pointer.
  static const URLUtil_Dev* Get();

  Var Canonicalize(const Var& url,
                   PP_URLComponents_Dev* components = NULL) const;

  Var ResolveRelativeToURL(const Var& base_url,
                           const Var& relative_string,
                           PP_URLComponents_Dev* components = NULL) const;
  Var ResolveRelativeToDocument(const InstanceHandle& instance,
                                const Var& relative_string,
                                PP_URLComponents_Dev* components = NULL) const;

  bool IsSameSecurityOrigin(const Var& url_a, const Var& url_b) const;
  bool DocumentCanRequest(const InstanceHandle& instance, const Var& url) const;
  bool DocumentCanAccessDocument(const InstanceHandle& active,
                                 const InstanceHandle& target) const;
  Var GetDocumentURL(const InstanceHandle& instance,
                     PP_URLComponents_Dev* components = NULL) const;

  Var GetPluginInstanceURL(const InstanceHandle& instance,
                           PP_URLComponents_Dev* components = NULL) const;
  Var GetPluginReferrerURL(const InstanceHandle& instance,
                           PP_URLComponents_Dev* components = NULL) const;

 private:
  URLUtil_Dev() {}

  // Copy and assignment are disallowed.
  URLUtil_Dev(const URLUtil_Dev& other);
  URLUtil_Dev& operator=(const URLUtil_Dev& other);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_URL_UTIL_DEV_H_
