// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_SERVICE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "url/gurl.h"

class OmahaXmlWriter;

// OmahaServiceProvider provides the OmahaService with URLs and other data that
// are necessary to send update requests.
class OmahaServiceProvider {
 public:
  OmahaServiceProvider();
  virtual ~OmahaServiceProvider();

  // Starts the provider.  This method will be called on the UI thread.
  virtual void Start();

  // Stops the provider.  This method will be called on the UI thread.
  virtual void Stop();

  // Returns the URL to use for update checks.
  virtual GURL GetUpdateServerURL() const;

  // Returns the unique ID for this application. The returned value may depend
  // on bundle id, channel, or form factor.
  virtual std::string GetApplicationID() const;

  // Returns the default brand code for this installation.  The returned value
  // may depend on bundle id, channel, or form factor.  Returns the empty string
  // if this installation is not associated with a brand code.
  virtual std::string GetBrandCode() const;

  // Allows the embedder to append embedder-specific attributes to the XML
  // request.  This method may be called multiple times per request.  Embedders
  // should only add attributes that are relevant for the given element |tag|.
  virtual void AppendExtraAttributes(const std::string& tag,
                                     OmahaXmlWriter* writer) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmahaServiceProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_OMAHA_OMAHA_SERVICE_PROVIDER_H_
